#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rcore/console.h"
#include "rcore/equiv.h"
#include "rcore/kernel_context_local.h"
#include "rcore/linker.h"
#include "rcore/syscalls.h"

#define STDOUT_FD 1
#define STDDEBUG_FD 2
#define APP_SLOT_SIZE 0x200000UL
#define APP_IMAGE_BASE 0x80400000UL
#define APP_STAGE_LIMIT 0x83000000UL
#define MONOTONIC_TICKS_PER_SECOND 1000000UL
#define ELF_MAGIC 0x464c457fU
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_PT_LOAD 1U
#define ELF_PF_X 0x1U
#define MAX_BATCH_APPS 32U

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct rcore_time_spec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

enum syscall_outcome {
    SYSCALL_CONTINUE,
    SYSCALL_EXIT,
    SYSCALL_UNSUPPORTED,
};

struct syscall_result {
    enum syscall_outcome kind;
    uintptr_t value;
};

struct staged_app_image {
    uintptr_t start;
    size_t size;
};

static inline uint64_t rcore_read_scause(void) {
    uint64_t value;
    asm volatile("csrr %0, scause" : "=r"(value));
    return value;
}

static inline uint64_t rcore_read_time(void) {
    uint64_t value;
    asm volatile("csrr %0, time" : "=r"(value));
    return value;
}

static inline void rcore_flush_icache(void) {
    asm volatile("fence.i" ::: "memory");
}

static inline void rcore_sbi_system_reset(void) {
    register uintptr_t a0 asm("a0") = 0;
    register uintptr_t a1 asm("a1") = 0;
    register uintptr_t a6 asm("a6") = 0;
    register uintptr_t a7 asm("a7") = 0x53525354UL;
    asm volatile("ecall"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a6), "r"(a7)
                 : "memory");
}

static void print_unsigned(size_t value) {
    char buf[32];
    size_t len = 0;
    if (value == 0) {
        rcore_console_putchar('0');
        return;
    }
    while (value > 0) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (len > 0) {
        rcore_console_putchar(buf[--len]);
    }
}

static void print_hex(uintptr_t value) {
    char buf[2 + sizeof(uintptr_t) * 2];
    static const char digits[] = "0123456789abcdef";
    buf[0] = '0';
    buf[1] = 'x';
    for (size_t i = 0; i < sizeof(uintptr_t) * 2; ++i) {
        size_t shift = (sizeof(uintptr_t) * 2 - 1 - i) * 4;
        unsigned int nibble = (unsigned int)((value >> shift) & 0xF);
        buf[2 + i] = digits[nibble];
    }
    for (size_t i = 0; i < 2 + sizeof(uintptr_t) * 2; ++i) {
        rcore_console_putchar(buf[i]);
    }
}

static void log_prefix(const char *level) {
    rcore_console_puts(level);
    rcore_console_puts(" ");
}

static const char *exception_name(uint64_t code) {
    switch (code) {
        case 0: return "InstructionMisaligned";
        case 1: return "InstructionFault";
        case 2: return "IllegalInstruction";
        case 3: return "Breakpoint";
        case 4: return "LoadMisaligned";
        case 5: return "LoadFault";
        case 6: return "StoreMisaligned";
        case 7: return "StoreFault";
        case 8: return "UserEnvCall";
        case 12: return "InstructionPageFault";
        case 13: return "LoadPageFault";
        case 15: return "StorePageFault";
        default: return "Unknown";
    }
}

static uintptr_t app_slot_base(size_t index) {
    return APP_IMAGE_BASE + index * APP_SLOT_SIZE;
}

static uintptr_t align_up_uintptr(uintptr_t value, uintptr_t align) {
    return (value + align - 1) & ~(align - 1);
}

static bool stage_all_apps(struct staged_app_image staged_apps[], size_t *staged_count_out) {
    size_t count = rcore_app_count();
    if (count == 0 || count > MAX_BATCH_APPS) {
        return false;
    }
    uintptr_t cursor = APP_IMAGE_BASE + count * APP_SLOT_SIZE;
    cursor = align_up_uintptr(cursor, 16);
    for (size_t i = 0; i < count; ++i) {
        struct rcore_app_meta meta = rcore_app_get(i);
        if (!rcore_app_meta_valid(&meta) || meta.start == 0 || meta.size == 0) {
            return false;
        }
        if (cursor > APP_STAGE_LIMIT || meta.size > (size_t)(APP_STAGE_LIMIT - cursor)) {
            return false;
        }
        memmove((void *)cursor, (const void *)meta.start, meta.size);
        staged_apps[i] = (struct staged_app_image){
            .start = cursor,
            .size = meta.size,
        };
        cursor = align_up_uintptr(cursor + meta.size, 16);
    }
    if (staged_count_out) {
        *staged_count_out = count;
    }
    return true;
}

static bool elf_header_valid(const struct elf64_ehdr *header, size_t image_size) {
    if (header == NULL || image_size < sizeof(*header)) {
        return false;
    }
    uint32_t magic = 0;
    memcpy(&magic, header->e_ident, sizeof(magic));
    if (magic != ELF_MAGIC) {
        return false;
    }
    if (header->e_ident[4] != ELF_CLASS64 || header->e_ident[5] != ELF_DATA2LSB) {
        return false;
    }
    if (header->e_phoff > image_size) {
        return false;
    }
    if (header->e_phentsize != sizeof(struct elf64_phdr)) {
        return false;
    }
    if ((size_t)header->e_phnum > SIZE_MAX / sizeof(struct elf64_phdr)) {
        return false;
    }
    size_t ph_size = (size_t)header->e_phnum * sizeof(struct elf64_phdr);
    return (size_t)header->e_phoff + ph_size <= image_size;
}

static bool load_elf_image(
    size_t index,
    const struct rcore_app_meta *meta,
    uintptr_t *entry_out,
    uintptr_t *stack_top_out
) {
    const struct elf64_ehdr *header = (const struct elf64_ehdr *)meta->start;
    if (!elf_header_valid(header, meta->size)) {
        return false;
    }
    const struct elf64_phdr *phdrs = (const struct elf64_phdr *)(meta->start + header->e_phoff);
    uintptr_t min_vaddr = UINTPTR_MAX;
    uintptr_t max_vaddr = 0;
    bool saw_load = false;
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const struct elf64_phdr *ph = &phdrs[i];
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_vaddr < min_vaddr) {
            min_vaddr = (uintptr_t)ph->p_vaddr;
        }
        if ((uintptr_t)ph->p_vaddr + ph->p_memsz > max_vaddr) {
            max_vaddr = (uintptr_t)ph->p_vaddr + (uintptr_t)ph->p_memsz;
        }
        saw_load = true;
    }
    if (!saw_load || min_vaddr == UINTPTR_MAX) {
        return false;
    }
    uintptr_t slot_base = app_slot_base(index);
    uintptr_t image_base = min_vaddr;
    uintptr_t delta = slot_base - image_base;
    memset((void *)slot_base, 0, APP_SLOT_SIZE);
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const struct elf64_phdr *ph = &phdrs[i];
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_offset > meta->size || ph->p_filesz > meta->size - ph->p_offset) {
            return false;
        }
        uintptr_t target = (uintptr_t)ph->p_vaddr + delta;
        if (target < slot_base || target + ph->p_memsz > slot_base + APP_SLOT_SIZE) {
            return false;
        }
        if (ph->p_filesz > 0) {
            memcpy((void *)target, (const void *)(meta->start + ph->p_offset), (size_t)ph->p_filesz);
        }
        if (ph->p_memsz > ph->p_filesz) {
            memset((void *)(target + ph->p_filesz), 0, (size_t)(ph->p_memsz - ph->p_filesz));
        }
    }
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const struct elf64_phdr *ph = &phdrs[i];
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz < sizeof(uintptr_t)) {
            continue;
        }
        if ((ph->p_flags & ELF_PF_X) != 0) {
            continue;
        }
        uintptr_t target = (uintptr_t)ph->p_vaddr + delta;
        size_t words = (size_t)(ph->p_memsz / sizeof(uintptr_t));
        uintptr_t *cursor = (uintptr_t *)target;
        for (size_t word = 0; word < words; ++word) {
            uintptr_t value = cursor[word];
            if (value >= min_vaddr && value < max_vaddr) {
                cursor[word] = value + delta;
            }
        }
    }
    if (entry_out) {
        *entry_out = (uintptr_t)header->e_entry + delta;
    }
    if (stack_top_out) {
        *stack_top_out = slot_base + APP_SLOT_SIZE;
    }
    return true;
}

static bool load_app_image(
    const struct staged_app_image staged_apps[],
    size_t staged_app_count,
    size_t index,
    uintptr_t *entry_out,
    uintptr_t *stack_top_out
) {
    if (index >= staged_app_count) {
        return false;
    }
    struct staged_app_image staged = staged_apps[index];
    struct rcore_app_meta meta = {staged.start, staged.size};
    if (meta.size == 0 || meta.size > APP_SLOT_SIZE) {
        return false;
    }
    if (load_elf_image(index, &meta, entry_out, stack_top_out)) {
        rcore_flush_icache();
        return true;
    }
    uintptr_t slot_base = app_slot_base(index);
    memset((void *)slot_base, 0, APP_SLOT_SIZE);
    memcpy((void *)slot_base, (const void *)meta.start, meta.size);
    if (entry_out) {
        *entry_out = slot_base;
    }
    if (stack_top_out) {
        *stack_top_out = slot_base + APP_SLOT_SIZE;
    }
    rcore_flush_icache();
    return true;
}

static void syscall_write_bytes(const char *ptr, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        rcore_console_putchar((unsigned char)ptr[i]);
    }
}

static bool handle_write(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t *written) {
    if (fd != STDOUT_FD && fd != STDDEBUG_FD) {
        return false;
    }
    syscall_write_bytes((const char *)buf, (size_t)count);
    *written = count;
    return true;
}

static bool handle_clock_gettime(uintptr_t clock_id, uintptr_t buf) {
    (void)clock_id;
    if (buf == 0) {
        return false;
    }
    uint64_t ticks = rcore_read_time();
    struct rcore_time_spec spec = {
        .tv_sec = (int64_t)(ticks / MONOTONIC_TICKS_PER_SECOND),
        .tv_nsec = (int64_t)((ticks % MONOTONIC_TICKS_PER_SECOND) * 1000UL),
    };
    *(struct rcore_time_spec *)buf = spec;
    return true;
}

static struct syscall_result handle_syscall(struct rcore_local_context *ctx) {
    uintptr_t args[6];
    for (size_t i = 0; i < 6; ++i) {
        args[i] = rcore_local_context_get_a(ctx, i);
    }
    uintptr_t id = rcore_local_context_get_a(ctx, 7);
    switch (id) {
        case RCORE_SYS_WRITE: {
            uintptr_t bytes = 0;
            if (!handle_write(args[0], args[1], args[2], &bytes)) {
                return (struct syscall_result){SYSCALL_UNSUPPORTED, id};
            }
            rcore_local_context_set_a(ctx, 0, bytes);
            rcore_local_context_move_next(ctx);
            return (struct syscall_result){SYSCALL_CONTINUE, 0};
        }
        case RCORE_SYS_CLOCK_GETTIME:
            if (!handle_clock_gettime(args[0], args[1])) {
                return (struct syscall_result){SYSCALL_UNSUPPORTED, id};
            }
            rcore_local_context_set_a(ctx, 0, 0);
            rcore_local_context_move_next(ctx);
            return (struct syscall_result){SYSCALL_CONTINUE, 0};
        case RCORE_SYS_SCHED_YIELD:
            rcore_local_context_set_a(ctx, 0, 0);
            rcore_local_context_move_next(ctx);
            return (struct syscall_result){SYSCALL_CONTINUE, 0};
        case RCORE_SYS_EXIT:
        case RCORE_SYS_EXIT_GROUP:
            return (struct syscall_result){SYSCALL_EXIT, args[0]};
        default:
            return (struct syscall_result){SYSCALL_UNSUPPORTED, id};
    }
}

static void run_batch(void) {
    size_t count = rcore_app_count();
    struct staged_app_image staged_apps[MAX_BATCH_APPS];
    size_t staged_app_count = 0;
    if (!stage_all_apps(staged_apps, &staged_app_count)) {
        log_prefix("[ERROR]");
        rcore_console_puts("failed to snapshot app images before execution\n");
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        uintptr_t entry = 0;
        uintptr_t stack_top = 0;
        if (!load_app_image(staged_apps, staged_app_count, i, &entry, &stack_top)) {
            log_prefix("[ERROR]");
            rcore_console_puts("failed to load app");
            print_unsigned(i);
            rcore_console_puts("\n\n");
            continue;
        }
        rcore_console_puts("load app");
        print_unsigned(i);
        rcore_console_puts(" to ");
        print_hex(entry);
        rcore_console_puts("\n");

        struct rcore_local_context ctx = rcore_local_context_user(entry);
        rcore_local_context_set_sp(&ctx, stack_top);
        for (;;) {
            (void)rcore_local_context_execute(&ctx);
            uint64_t scause = rcore_read_scause();
            bool is_interrupt = (scause >> 63) != 0;
            uint64_t code = scause & ((((uint64_t)1) << 63) - 1);
            if (!is_interrupt && code == 8) {
                struct syscall_result result = handle_syscall(&ctx);
                if (result.kind == SYSCALL_CONTINUE) {
                    continue;
                }
                if (result.kind == SYSCALL_EXIT) {
                    log_prefix("[ INFO]");
                    rcore_console_puts("app");
                    print_unsigned(i);
                    rcore_console_puts(" exit with code ");
                    print_unsigned(result.value);
                    rcore_console_puts("\n");
                } else {
                    log_prefix("[ERROR]");
                    rcore_console_puts("app");
                    print_unsigned(i);
                    rcore_console_puts(" call an unsupported syscall ");
                    print_unsigned(result.value);
                    rcore_console_puts("\n");
                }
            } else {
                log_prefix("[ERROR]");
                rcore_console_puts("app");
                print_unsigned(i);
                rcore_console_puts(" was killed because of Exception(");
                rcore_console_puts(exception_name(code));
                rcore_console_puts(")\n");
            }
            rcore_flush_icache();
            break;
        }
        rcore_console_puts("\n");
    }
}

__attribute__((noreturn)) void rust_main(void) {
    struct rcore_kernel_layout layout = rcore_kernel_layout_locate();
    rcore_zero_bss(&layout);
    init_console();
    set_log_level(NULL);
    test_log();
    rcore_equiv_emit("boot", "ch4", "enter", "ok", "lang=c,chapter=4");
    run_batch();
    rcore_sbi_system_reset();
    for (;;) {
        asm volatile("wfi");
    }
}

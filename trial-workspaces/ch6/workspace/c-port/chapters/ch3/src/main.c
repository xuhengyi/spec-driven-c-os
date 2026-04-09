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
#define MONOTONIC_TICKS_PER_SECOND 1000000UL

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

static bool load_app_image(size_t index, uintptr_t *entry_out, uintptr_t *stack_top_out) {
    struct rcore_app_meta meta = rcore_app_get(index);
    if (!rcore_app_meta_valid(&meta) || meta.start == 0) {
        return false;
    }
    if (meta.size == 0 || meta.size > APP_SLOT_SIZE) {
        return false;
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
    for (size_t i = 0; i < count; ++i) {
        uintptr_t entry = 0;
        uintptr_t stack_top = 0;
        if (!load_app_image(i, &entry, &stack_top)) {
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
    rcore_equiv_emit("boot", "ch3", "enter", "ok", "lang=c,chapter=3");
    run_batch();
    rcore_sbi_system_reset();
    for (;;) {
        asm volatile("wfi");
    }
}

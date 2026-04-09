#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rcore/console.h"
#include "rcore/equiv.h"
#include "rcore/kernel_context_local.h"
#include "rcore/linker.h"
#include "rcore/syscalls.h"

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDDEBUG_FD 2
#define FIRST_USER_FD 3

#define EXEC_SLOT_BASE 0x80400000UL
#define PROCESS_SLOT_SIZE 0x00080000UL
#define PROCESS_STORE_BASE 0x81000000UL
#define MAX_PROCESS_COUNT 40U
#define KERNEL_STATE_BASE 0x82400000UL
#define APP_STAGE_BASE 0x82600000UL
#define APP_STAGE_LIMIT 0x84000000UL
#define MAX_APP_COUNT 32U
#define MAX_APP_NAME 32U

#define MAX_FILE_COUNT 16U
#define MAX_FILE_NAME 32U
#define MAX_FILE_BYTES 4096U
#define MAX_FD_COUNT 16U
#define OPEN_MODE_MASK 0x3UL
#define MAX_SIGNAL_NO 31U
#define MAX_SIGNAL_DEPTH 4U

#define SIGINT_NO 2U
#define SIGKILL_NO 9U
#define SIGUSR1_NO 10U
#define SIGALRM_NO 14U
#define SIGCONT_NO 18U
#define SIGSTOP_NO 19U
#define TIMER_SLICE_TICKS 200000UL

#define MONOTONIC_TICKS_PER_SECOND 1000000UL
#define ELF_MAGIC 0x464c457fU
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_PT_LOAD 1U
#define ELF_PF_X 0x1U

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

enum proc_state {
    PROC_UNUSED = 0,
    PROC_RUNNABLE,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_STOPPED,
    PROC_ZOMBIE,
};

enum fd_rights {
    FD_RIGHT_READ = 1U << 0,
    FD_RIGHT_WRITE = 1U << 1,
};

struct staged_app_image {
    uintptr_t start;
    size_t size;
    char name[MAX_APP_NAME];
};

struct fd_entry {
    bool used;
    uint32_t rights;
    size_t file_index;
    size_t offset;
};

struct file_node {
    bool used;
    char name[MAX_FILE_NAME];
    size_t size;
    unsigned char data[MAX_FILE_BYTES];
};

struct signal_action {
    uintptr_t handler;
    uint64_t mask;
    uintptr_t flags;
};

struct saved_signal_context {
    int signum;
    struct rcore_local_context ctx;
};

struct process {
    bool used;
    int pid;
    int parent_pid;
    enum proc_state state;
    int exit_code;
    int wait_pid;
    uintptr_t wait_status_ptr;
    uintptr_t store_base;
    struct rcore_local_context ctx;
    struct fd_entry fds[MAX_FD_COUNT];
    uint64_t signal_mask;
    uint64_t pending_signals;
    size_t signal_depth;
    struct signal_action signal_actions[MAX_SIGNAL_NO + 1];
    struct saved_signal_context signal_stack[MAX_SIGNAL_DEPTH];
};

struct kernel_state {
    size_t app_count;
    int next_pid;
    size_t current_index;
    bool has_current;
    struct staged_app_image apps[MAX_APP_COUNT];
    struct process procs[MAX_PROCESS_COUNT];
    struct file_node files[MAX_FILE_COUNT];
};

static const char *const CH7_APP_NAMES[] = {
    "00hello_world",
    "01store_fault",
    "02power",
    "03priv_inst",
    "04priv_csr",
    "12forktest",
    "13forktree",
    "14forktest2",
    "15matrix",
    "user_shell",
    "initproc",
    "filetest_simple",
    "cat_filea",
    "sig_simple",
    "sig_simple2",
    "sig_ctrlc",
    "sig_tests",
};

static inline uint64_t rcore_read_scause(void) {
    uint64_t value;
    asm volatile("csrr %0, scause" : "=r"(value));
    return value;
}

static inline uint64_t rcore_read_stval(void) {
    uint64_t value;
    asm volatile("csrr %0, stval" : "=r"(value));
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

static inline long rcore_sbi_console_getchar(void) {
    register long a0 asm("a0");
    register long a7 asm("a7") = 2;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline void rcore_sbi_set_timer(uint64_t value) {
    register uint64_t a0 asm("a0") = value;
    register uint64_t a1 asm("a1") = 0;
    register uint64_t a6 asm("a6") = 0;
    register uint64_t a7 asm("a7") = 0x54494D45UL;
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
    if ((long)a0 < 0) {
        register uint64_t legacy_a0 asm("a0") = value;
        register uint64_t legacy_a7 asm("a7") = 0;
        asm volatile("ecall" : "+r"(legacy_a0) : "r"(legacy_a7) : "memory");
    }
}

static inline void rcore_enable_timer_interrupt(void) {
    uintptr_t value;
    asm volatile("csrr %0, sie" : "=r"(value));
    value |= (1UL << 5);
    asm volatile("csrw sie, %0" : : "r"(value) : "memory");
}

static inline void rcore_program_timer_slice(void) {
    rcore_sbi_set_timer(rcore_read_time() + TIMER_SLICE_TICKS);
}

static inline void rcore_cancel_timer_slice(void) {
    rcore_sbi_set_timer(UINT64_MAX);
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
    for (size_t i = 0; i < sizeof(buf); ++i) {
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

static uintptr_t align_up_uintptr(uintptr_t value, uintptr_t align) {
    return (value + align - 1) & ~(align - 1);
}

static size_t min_size(size_t lhs, size_t rhs) {
    return lhs < rhs ? lhs : rhs;
}

static struct kernel_state *kernel_state(void) {
    return (struct kernel_state *)KERNEL_STATE_BASE;
}

static uintptr_t process_store_base(size_t index) {
    return PROCESS_STORE_BASE + index * PROCESS_SLOT_SIZE;
}

static void copy_c_string(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (cap == 0) {
        return;
    }
    while (i + 1 < cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static bool str_equal(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static bool signal_valid(long signum) {
    return signum > 0 && signum <= (long)MAX_SIGNAL_NO;
}

static uint64_t signal_bit(long signum) {
    return 1ULL << (unsigned long)signum;
}

static bool signal_is_masked(const struct process *proc, long signum) {
    return (proc->signal_mask & signal_bit(signum)) != 0;
}

static bool signal_is_unmaskable(long signum) {
    return signum == (long)SIGKILL_NO || signum == (long)SIGCONT_NO;
}

static bool signal_is_stop(long signum) {
    return signum == (long)SIGSTOP_NO;
}

static bool signal_is_continue(long signum) {
    return signum == (long)SIGCONT_NO;
}

static bool signal_is_fatal(long signum) {
    return signum == (long)SIGKILL_NO;
}

static int next_deliverable_signal(const struct process *proc) {
    for (long signum = 1; signum <= (long)MAX_SIGNAL_NO; ++signum) {
        if ((proc->pending_signals & signal_bit(signum)) == 0) {
            continue;
        }
        if (!signal_is_unmaskable(signum) && signal_is_masked(proc, signum)) {
            continue;
        }
        return (int)signum;
    }
    return -1;
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
    return (size_t)header->e_phoff + (size_t)header->e_phnum * sizeof(struct elf64_phdr) <= image_size;
}

static bool stage_all_apps(struct kernel_state *ks) {
    size_t count = rcore_app_count();
    if (count == 0 || count > MAX_APP_COUNT || count > (sizeof(CH7_APP_NAMES) / sizeof(CH7_APP_NAMES[0]))) {
        return false;
    }
    uintptr_t cursor = APP_STAGE_BASE;
    for (size_t i = 0; i < count; ++i) {
        struct rcore_app_meta meta = rcore_app_get(i);
        if (!rcore_app_meta_valid(&meta) || meta.start == 0 || meta.size == 0) {
            return false;
        }
        if (cursor > APP_STAGE_LIMIT || meta.size > (size_t)(APP_STAGE_LIMIT - cursor)) {
            return false;
        }
        memmove((void *)cursor, (const void *)meta.start, meta.size);
        ks->apps[i].start = cursor;
        ks->apps[i].size = meta.size;
        copy_c_string(ks->apps[i].name, sizeof(ks->apps[i].name), CH7_APP_NAMES[i]);
        cursor = align_up_uintptr(cursor + meta.size, 16);
    }
    ks->app_count = count;
    return true;
}

static int find_app_index_by_name(const struct kernel_state *ks, const char *name) {
    for (size_t i = 0; i < ks->app_count; ++i) {
        if (str_equal(ks->apps[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

static int find_app_index_by_user_prefix(const struct kernel_state *ks, uintptr_t user_ptr) {
    if (user_ptr < EXEC_SLOT_BASE || user_ptr >= EXEC_SLOT_BASE + PROCESS_SLOT_SIZE) {
        return -1;
    }
    const char *src = (const char *)user_ptr;
    for (size_t i = 0; i < ks->app_count; ++i) {
        const char *name = ks->apps[i].name;
        size_t len = 0;
        while (name[len] != '\0') {
            if (src[len] != name[len]) {
                break;
            }
            ++len;
        }
        if (name[len] == '\0') {
            return (int)i;
        }
    }
    return -1;
}

static int alloc_file_index(struct kernel_state *ks) {
    for (size_t i = 0; i < MAX_FILE_COUNT; ++i) {
        if (!ks->files[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int find_file_index_by_name(const struct kernel_state *ks, const char *name) {
    for (size_t i = 0; i < MAX_FILE_COUNT; ++i) {
        if (ks->files[i].used && str_equal(ks->files[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

static int alloc_fd(struct process *proc) {
    for (size_t i = FIRST_USER_FD; i < MAX_FD_COUNT; ++i) {
        if (!proc->fds[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static struct fd_entry *lookup_fd(struct process *proc, uintptr_t fd) {
    if (fd >= MAX_FD_COUNT || fd < FIRST_USER_FD) {
        return NULL;
    }
    if (!proc->fds[fd].used) {
        return NULL;
    }
    return &proc->fds[fd];
}

static bool load_elf_into_exec_slot(
    const struct staged_app_image *app,
    uintptr_t *entry_out,
    uintptr_t *stack_top_out
) {
    const struct elf64_ehdr *header = (const struct elf64_ehdr *)app->start;
    if (!elf_header_valid(header, app->size)) {
        return false;
    }
    const struct elf64_phdr *phdrs = (const struct elf64_phdr *)(app->start + header->e_phoff);
    uintptr_t min_vaddr = UINTPTR_MAX;
    uintptr_t max_vaddr = 0;
    bool saw_load = false;
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const struct elf64_phdr *ph = &phdrs[i];
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if ((uintptr_t)ph->p_vaddr < min_vaddr) {
            min_vaddr = (uintptr_t)ph->p_vaddr;
        }
        if ((uintptr_t)ph->p_vaddr + (uintptr_t)ph->p_memsz > max_vaddr) {
            max_vaddr = (uintptr_t)ph->p_vaddr + (uintptr_t)ph->p_memsz;
        }
        saw_load = true;
    }
    if (!saw_load || min_vaddr == UINTPTR_MAX) {
        return false;
    }

    uintptr_t delta = EXEC_SLOT_BASE - min_vaddr;
    memset((void *)EXEC_SLOT_BASE, 0, PROCESS_SLOT_SIZE);
    for (uint16_t i = 0; i < header->e_phnum; ++i) {
        const struct elf64_phdr *ph = &phdrs[i];
        if (ph->p_type != ELF_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_offset > app->size || ph->p_filesz > app->size - ph->p_offset) {
            return false;
        }
        uintptr_t target = (uintptr_t)ph->p_vaddr + delta;
        if (target < EXEC_SLOT_BASE || target + (uintptr_t)ph->p_memsz > EXEC_SLOT_BASE + PROCESS_SLOT_SIZE) {
            return false;
        }
        if (ph->p_filesz > 0) {
            memcpy((void *)target, (const void *)(app->start + ph->p_offset), (size_t)ph->p_filesz);
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
        *stack_top_out = EXEC_SLOT_BASE + PROCESS_SLOT_SIZE;
    }
    rcore_flush_icache();
    return true;
}

static void set_syscall_return(struct rcore_local_context *ctx, long value) {
    rcore_local_context_set_a(ctx, 0, (uintptr_t)value);
}

static int alloc_process_index(struct kernel_state *ks) {
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (!ks->procs[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int find_process_index_by_pid(const struct kernel_state *ks, int pid) {
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        if (ks->procs[i].used && ks->procs[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static int next_pid(struct kernel_state *ks) {
    return ks->next_pid++;
}

static void clear_process(struct process *proc) {
    memset(proc, 0, sizeof(*proc));
    proc->state = PROC_UNUSED;
    proc->wait_pid = -1;
}

static void copy_exec_to_store(const struct process *proc) {
    memcpy((void *)proc->store_base, (const void *)EXEC_SLOT_BASE, PROCESS_SLOT_SIZE);
}

static void restore_store_to_exec(const struct process *proc) {
    memcpy((void *)EXEC_SLOT_BASE, (const void *)proc->store_base, PROCESS_SLOT_SIZE);
    rcore_flush_icache();
}

static void *saved_user_ptr(const struct process *proc, uintptr_t user_ptr, size_t size) {
    if (user_ptr < EXEC_SLOT_BASE || size > PROCESS_SLOT_SIZE) {
        return NULL;
    }
    uintptr_t offset = user_ptr - EXEC_SLOT_BASE;
    if (offset > PROCESS_SLOT_SIZE || size > PROCESS_SLOT_SIZE - offset) {
        return NULL;
    }
    return (void *)(proc->store_base + offset);
}

static void *current_user_ptr(uintptr_t user_ptr, size_t size) {
    if (user_ptr < EXEC_SLOT_BASE || size > PROCESS_SLOT_SIZE) {
        return NULL;
    }
    uintptr_t offset = user_ptr - EXEC_SLOT_BASE;
    if (offset > PROCESS_SLOT_SIZE || size > PROCESS_SLOT_SIZE - offset) {
        return NULL;
    }
    return (void *)(EXEC_SLOT_BASE + offset);
}

static bool read_current_user_string(uintptr_t user_ptr, char *out, size_t cap) {
    if (cap == 0) {
        return false;
    }
    if (user_ptr < EXEC_SLOT_BASE || user_ptr >= EXEC_SLOT_BASE + PROCESS_SLOT_SIZE) {
        return false;
    }
    const char *src = (const char *)user_ptr;
    size_t i = 0;
    while (i + 1 < cap) {
        char c = src[i];
        out[i] = c;
        if (c == '\0') {
            return true;
        }
        ++i;
    }
    out[cap - 1] = '\0';
    return false;
}

static bool looks_like_user_ptr(uintptr_t value) {
    return value >= EXEC_SLOT_BASE && value < EXEC_SLOT_BASE + PROCESS_SLOT_SIZE;
}

static uint32_t fd_rights_from_open_flags(uintptr_t flags) {
    switch (flags & OPEN_MODE_MASK) {
        case 0:
            return FD_RIGHT_READ;
        case 1:
            return FD_RIGHT_WRITE;
        case 2:
            return FD_RIGHT_READ | FD_RIGHT_WRITE;
        default:
            return FD_RIGHT_READ | FD_RIGHT_WRITE;
    }
}

static bool open_flags_create(uintptr_t flags) {
    return (flags & ~((uintptr_t)OPEN_MODE_MASK)) != 0;
}

static int spawn_process_from_app(struct kernel_state *ks, int app_index, int parent_pid, int forced_pid) {
    if (app_index < 0 || (size_t)app_index >= ks->app_count) {
        return -1;
    }
    int index = alloc_process_index(ks);
    if (index < 0) {
        return -1;
    }
    uintptr_t entry = 0;
    uintptr_t stack_top = 0;
    if (!load_elf_into_exec_slot(&ks->apps[app_index], &entry, &stack_top)) {
        return -1;
    }
    struct process *proc = &ks->procs[index];
    clear_process(proc);
    proc->used = true;
    proc->pid = (forced_pid > 0) ? forced_pid : next_pid(ks);
    if (forced_pid > 0 && ks->next_pid <= forced_pid) {
        ks->next_pid = forced_pid + 1;
    }
    proc->parent_pid = parent_pid;
    proc->state = PROC_RUNNABLE;
    proc->wait_pid = -1;
    proc->store_base = process_store_base((size_t)index);
    proc->ctx = rcore_local_context_user(entry);
    rcore_local_context_set_sp(&proc->ctx, stack_top);
    copy_exec_to_store(proc);
    return proc->pid;
}

static bool child_matches_wait(const struct process *parent, int child_pid) {
    return parent->wait_pid < 0 || parent->wait_pid == child_pid;
}

static int find_zombie_child_index(const struct kernel_state *ks, int parent_pid, int waited_pid) {
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        const struct process *child = &ks->procs[i];
        if (!child->used || child->state != PROC_ZOMBIE || child->parent_pid != parent_pid) {
            continue;
        }
        if (waited_pid >= 0 && child->pid != waited_pid) {
            continue;
        }
        return (int)i;
    }
    return -1;
}

static bool has_live_child(const struct kernel_state *ks, int parent_pid, int waited_pid) {
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        const struct process *child = &ks->procs[i];
        if (!child->used || child->parent_pid != parent_pid) {
            continue;
        }
        if (waited_pid >= 0 && child->pid != waited_pid) {
            continue;
        }
        return true;
    }
    return false;
}

static void reap_process(struct process *proc) {
    clear_process(proc);
}

static bool wake_waiting_parent(struct kernel_state *ks, const struct process *child) {
    int parent_index = find_process_index_by_pid(ks, child->parent_pid);
    if (parent_index < 0) {
        return false;
    }
    struct process *parent = &ks->procs[parent_index];
    if (!parent->used || parent->state != PROC_WAITING || !child_matches_wait(parent, child->pid)) {
        return false;
    }
    if (parent->wait_status_ptr != 0) {
        int *status_ptr = (int *)saved_user_ptr(parent, parent->wait_status_ptr, sizeof(int));
        if (status_ptr != NULL) {
            *status_ptr = child->exit_code;
        }
    }
    set_syscall_return(&parent->ctx, child->pid);
    parent->state = PROC_RUNNABLE;
    parent->wait_pid = -1;
    parent->wait_status_ptr = 0;
    return true;
}

static void finish_process(struct kernel_state *ks, size_t index, int exit_code) {
    struct process *proc = &ks->procs[index];
    proc->exit_code = exit_code;
    proc->state = PROC_ZOMBIE;
    if (wake_waiting_parent(ks, proc)) {
        reap_process(proc);
    }
}

static void enqueue_signal(struct process *proc, long signum) {
    proc->pending_signals |= signal_bit(signum);
}

static void clear_pending_signal(struct process *proc, long signum) {
    proc->pending_signals &= ~signal_bit(signum);
}

static bool begin_signal_handler(struct process *proc, long signum) {
    if (proc->signal_depth >= MAX_SIGNAL_DEPTH) {
        return false;
    }
    struct saved_signal_context *frame = &proc->signal_stack[proc->signal_depth++];
    frame->signum = (int)signum;
    frame->ctx = proc->ctx;
    proc->ctx.sepc = proc->signal_actions[signum].handler;
    rcore_local_context_set_a(&proc->ctx, 0, (uintptr_t)signum);
    return true;
}

static bool prepare_signal_delivery(struct kernel_state *ks, size_t index) {
    struct process *proc = &ks->procs[index];
    for (;;) {
        int signum = next_deliverable_signal(proc);
        if (signum < 0) {
            return true;
        }
        clear_pending_signal(proc, signum);
        if (signal_is_continue(signum)) {
            if (proc->state == PROC_STOPPED) {
                proc->state = PROC_RUNNABLE;
            }
            continue;
        }
        if (signal_is_stop(signum)) {
            proc->state = PROC_STOPPED;
            return false;
        }
        if (signal_is_fatal(signum)) {
            finish_process(ks, index, -((int)signum));
            return false;
        }
        if (proc->signal_actions[signum].handler != 0) {
            if (!begin_signal_handler(proc, signum)) {
                finish_process(ks, index, -((int)signum));
                return false;
            }
            return true;
        }
        finish_process(ks, index, -((int)signum));
        return false;
    }
}

static void log_trap_and_finish(struct kernel_state *ks, size_t index, uint64_t scause, uint64_t stval) {
    struct process *proc = &ks->procs[index];
    uint64_t code = scause & ((((uint64_t)1) << 63) - 1);
    log_prefix("[ERROR]");
    rcore_console_puts("Trap Exception(");
    rcore_console_puts(exception_name(code));
    rcore_console_puts(") stval=");
    print_hex((uintptr_t)stval);
    rcore_console_puts(" pc=");
    print_hex(proc->ctx.sepc);
    rcore_console_puts("\n");
    finish_process(ks, index, -3);
}

static int pick_next_runnable(const struct kernel_state *ks) {
    size_t start = ks->has_current ? (ks->current_index + 1) % MAX_PROCESS_COUNT : 0;
    for (size_t offset = 0; offset < MAX_PROCESS_COUNT; ++offset) {
        size_t index = (start + offset) % MAX_PROCESS_COUNT;
        const struct process *proc = &ks->procs[index];
        if (proc->used && proc->state == PROC_RUNNABLE) {
            return (int)index;
        }
    }
    return -1;
}

static bool handle_openat(struct kernel_state *ks, struct process *proc, uintptr_t path_ptr, uintptr_t flags) {
    char name[MAX_FILE_NAME];
    if (!read_current_user_string(path_ptr, name, sizeof(name))) {
        return false;
    }

    int file_index = find_file_index_by_name(ks, name);
    uint32_t rights = fd_rights_from_open_flags(flags);
    bool wants_create = open_flags_create(flags);
    if (file_index < 0) {
        if (!wants_create) {
            return false;
        }
        file_index = alloc_file_index(ks);
        if (file_index < 0) {
            return false;
        }
        struct file_node *node = &ks->files[file_index];
        memset(node, 0, sizeof(*node));
        node->used = true;
        copy_c_string(node->name, sizeof(node->name), name);
    } else if (wants_create && (rights & FD_RIGHT_WRITE) != 0) {
        ks->files[file_index].size = 0;
    }

    int fd = alloc_fd(proc);
    if (fd < 0) {
        return false;
    }
    proc->fds[fd].used = true;
    proc->fds[fd].rights = rights;
    proc->fds[fd].file_index = (size_t)file_index;
    proc->fds[fd].offset = 0;
    set_syscall_return(&proc->ctx, fd);
    rcore_local_context_move_next(&proc->ctx);
    return true;
}

static void handle_close(struct process *proc, uintptr_t fd) {
    if (fd < FIRST_USER_FD) {
        set_syscall_return(&proc->ctx, 0);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    struct fd_entry *entry = lookup_fd(proc, fd);
    if (entry == NULL) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    memset(entry, 0, sizeof(*entry));
    set_syscall_return(&proc->ctx, 0);
    rcore_local_context_move_next(&proc->ctx);
}

static bool handle_write(struct kernel_state *ks, struct process *proc, uintptr_t fd, uintptr_t buf, uintptr_t count) {
    struct rcore_local_context *ctx = &proc->ctx;
    const unsigned char *ptr = (const unsigned char *)current_user_ptr(buf, (size_t)count);
    if (ptr == NULL) {
        return false;
    }

    if (fd == STDOUT_FD || fd == STDDEBUG_FD) {
        for (size_t i = 0; i < (size_t)count; ++i) {
            rcore_console_putchar(ptr[i]);
        }
        set_syscall_return(ctx, (long)count);
        rcore_local_context_move_next(ctx);
        return true;
    }

    struct fd_entry *entry = lookup_fd(proc, fd);
    if (entry == NULL || (entry->rights & FD_RIGHT_WRITE) == 0) {
        return false;
    }
    if (entry->file_index >= MAX_FILE_COUNT) {
        return false;
    }
    struct file_node *node = &ks->files[entry->file_index];
    if (!node->used) {
        return false;
    }
    if (entry->offset > MAX_FILE_BYTES) {
        return false;
    }
    size_t available = MAX_FILE_BYTES - entry->offset;
    size_t written = min_size((size_t)count, available);
    if (written > 0) {
        memcpy(node->data + entry->offset, ptr, written);
        entry->offset += written;
        if (entry->offset > node->size) {
            node->size = entry->offset;
        }
    }
    set_syscall_return(ctx, (long)written);
    rcore_local_context_move_next(ctx);
    return true;
}

static bool handle_read(struct kernel_state *ks, struct process *proc, uintptr_t fd, uintptr_t buf, uintptr_t count) {
    struct rcore_local_context *ctx = &proc->ctx;
    unsigned char *ptr = (unsigned char *)current_user_ptr(buf, (size_t)count);
    if (ptr == NULL) {
        return false;
    }

    if (fd == STDIN_FD) {
        for (size_t i = 0; i < (size_t)count; ++i) {
            long ch;
            do {
                ch = rcore_sbi_console_getchar();
            } while (ch < 0);
            ptr[i] = (unsigned char)ch;
        }
        set_syscall_return(ctx, (long)count);
        rcore_local_context_move_next(ctx);
        return true;
    }

    struct fd_entry *entry = lookup_fd(proc, fd);
    if (entry == NULL || (entry->rights & FD_RIGHT_READ) == 0) {
        return false;
    }
    if (entry->file_index >= MAX_FILE_COUNT) {
        return false;
    }
    const struct file_node *node = &ks->files[entry->file_index];
    if (!node->used) {
        return false;
    }
    if (entry->offset > node->size) {
        return false;
    }
    size_t available = node->size - entry->offset;
    size_t copied = min_size((size_t)count, available);
    if (copied > 0) {
        memcpy(ptr, node->data + entry->offset, copied);
        entry->offset += copied;
    }
    set_syscall_return(ctx, (long)copied);
    rcore_local_context_move_next(ctx);
    return true;
}

static bool handle_clock_gettime(uintptr_t buf, struct rcore_local_context *ctx) {
    struct rcore_time_spec *spec = (struct rcore_time_spec *)current_user_ptr(buf, sizeof(struct rcore_time_spec));
    if (spec == NULL) {
        return false;
    }
    uint64_t ticks = rcore_read_time();
    spec->tv_sec = (int64_t)(ticks / MONOTONIC_TICKS_PER_SECOND);
    spec->tv_nsec = (int64_t)((ticks % MONOTONIC_TICKS_PER_SECOND) * 1000UL);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
    return true;
}

static void handle_getpid(struct process *proc) {
    set_syscall_return(&proc->ctx, proc->pid);
    rcore_local_context_move_next(&proc->ctx);
}

static void handle_sched_yield(struct process *proc) {
    set_syscall_return(&proc->ctx, 0);
    rcore_local_context_move_next(&proc->ctx);
}

static void handle_exit(struct kernel_state *ks, size_t index, long exit_code) {
    finish_process(ks, index, (int)exit_code);
}

static void handle_wait4(struct kernel_state *ks, size_t index, long waited_pid, uintptr_t status_ptr) {
    struct process *proc = &ks->procs[index];
    int zombie_index = find_zombie_child_index(ks, proc->pid, (int)waited_pid);
    if (zombie_index >= 0) {
        struct process *child = &ks->procs[zombie_index];
        if (status_ptr != 0) {
            int *status = (int *)current_user_ptr(status_ptr, sizeof(int));
            if (status != NULL) {
                *status = child->exit_code;
            }
        }
        set_syscall_return(&proc->ctx, child->pid);
        rcore_local_context_move_next(&proc->ctx);
        reap_process(child);
        return;
    }
    if (!has_live_child(ks, proc->pid, (int)waited_pid)) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    rcore_local_context_move_next(&proc->ctx);
    proc->state = PROC_WAITING;
    proc->wait_pid = (int)waited_pid;
    proc->wait_status_ptr = status_ptr;
}

static void handle_fork(struct kernel_state *ks, size_t index) {
    struct process *parent = &ks->procs[index];
    int child_index = alloc_process_index(ks);
    if (child_index < 0) {
        set_syscall_return(&parent->ctx, -1);
        rcore_local_context_move_next(&parent->ctx);
        return;
    }
    struct process *child = &ks->procs[child_index];
    clear_process(child);
    child->used = true;
    child->pid = next_pid(ks);
    child->parent_pid = parent->pid;
    child->state = PROC_RUNNABLE;
    child->wait_pid = -1;
    child->store_base = process_store_base((size_t)child_index);
    memcpy((void *)child->store_base, (const void *)EXEC_SLOT_BASE, PROCESS_SLOT_SIZE);
    memcpy(child->fds, parent->fds, sizeof(child->fds));
    child->signal_mask = parent->signal_mask;
    memcpy(child->signal_actions, parent->signal_actions, sizeof(child->signal_actions));
    child->pending_signals = 0;
    child->signal_depth = 0;
    child->ctx = parent->ctx;
    rcore_local_context_move_next(&child->ctx);
    set_syscall_return(&child->ctx, 0);

    set_syscall_return(&parent->ctx, child->pid);
    rcore_local_context_move_next(&parent->ctx);
}

static void handle_execve(struct kernel_state *ks, size_t index, uintptr_t path_ptr) {
    struct process *proc = &ks->procs[index];
    char name[MAX_APP_NAME];
    if (!read_current_user_string(path_ptr, name, sizeof(name))) {
        copy_c_string(name, sizeof(name), "<slice>");
    }
    int app_index = find_app_index_by_name(ks, name);
    if (app_index < 0) {
        app_index = find_app_index_by_user_prefix(ks, path_ptr);
    }
    if (app_index < 0) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    uintptr_t entry = 0;
    uintptr_t stack_top = 0;
    if (!load_elf_into_exec_slot(&ks->apps[app_index], &entry, &stack_top)) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    proc->ctx = rcore_local_context_user(entry);
    rcore_local_context_set_sp(&proc->ctx, stack_top);
}

static void handle_kill(struct kernel_state *ks, size_t index, long target_pid, long signum) {
    struct process *caller = &ks->procs[index];
    if (!signal_valid(signum)) {
        set_syscall_return(&caller->ctx, -1);
        rcore_local_context_move_next(&caller->ctx);
        return;
    }
    int target_index = find_process_index_by_pid(ks, (int)target_pid);
    if (target_index < 0) {
        set_syscall_return(&caller->ctx, -1);
        rcore_local_context_move_next(&caller->ctx);
        return;
    }
    struct process *target = &ks->procs[target_index];
    if (!target->used || target->state == PROC_ZOMBIE) {
        set_syscall_return(&caller->ctx, -1);
        rcore_local_context_move_next(&caller->ctx);
        return;
    }

    if (signal_is_fatal(signum)) {
        finish_process(ks, (size_t)target_index, -((int)signum));
    } else if (signal_is_continue(signum)) {
        clear_pending_signal(target, signum);
        if (target->state == PROC_STOPPED) {
            target->state = PROC_RUNNABLE;
        }
    } else {
        enqueue_signal(target, signum);
    }
    set_syscall_return(&caller->ctx, 0);
    rcore_local_context_move_next(&caller->ctx);
}

static void handle_sigprocmask(struct process *proc, uint64_t new_mask) {
    uint64_t old_mask = proc->signal_mask;
    proc->signal_mask = new_mask;
    set_syscall_return(&proc->ctx, (long)old_mask);
    rcore_local_context_move_next(&proc->ctx);
}

static void handle_sigaction(struct process *proc, long signum, uintptr_t act_ptr, uintptr_t old_ptr) {
    if (!signal_valid(signum) || signum == (long)SIGKILL_NO) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }

    struct signal_action *old_action = NULL;
    struct signal_action *new_action = NULL;
    if (old_ptr != 0) {
        old_action = (struct signal_action *)current_user_ptr(old_ptr, sizeof(struct signal_action));
        if (old_action == NULL) {
            set_syscall_return(&proc->ctx, -1);
            rcore_local_context_move_next(&proc->ctx);
            return;
        }
    }
    if (act_ptr != 0) {
        new_action = (struct signal_action *)current_user_ptr(act_ptr, sizeof(struct signal_action));
        if (new_action == NULL) {
            set_syscall_return(&proc->ctx, -1);
            rcore_local_context_move_next(&proc->ctx);
            return;
        }
    }
    if (old_action != NULL) {
        *old_action = proc->signal_actions[signum];
    }
    if (new_action != NULL) {
        proc->signal_actions[signum] = *new_action;
    }
    set_syscall_return(&proc->ctx, 0);
    rcore_local_context_move_next(&proc->ctx);
}

static void handle_sigreturn(struct kernel_state *ks, size_t index) {
    struct process *proc = &ks->procs[index];
    if (proc->signal_depth == 0) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    struct saved_signal_context *frame = &proc->signal_stack[proc->signal_depth - 1];
    proc->ctx = frame->ctx;
    proc->signal_depth--;
}

static void handle_syscall(struct kernel_state *ks, size_t index) {
    struct process *proc = &ks->procs[index];
    struct rcore_local_context *ctx = &proc->ctx;
    uintptr_t args[6];
    for (size_t i = 0; i < 6; ++i) {
        args[i] = rcore_local_context_get_a(ctx, i);
    }
    uintptr_t id = rcore_local_context_get_a(ctx, 7);
    switch (id) {
        case RCORE_SYS_OPENAT:
        {
            uintptr_t path_ptr = args[1];
            uintptr_t flags = args[2];
            if (!looks_like_user_ptr(path_ptr) && looks_like_user_ptr(args[0])) {
                path_ptr = args[0];
                flags = args[1];
            }
            if (!handle_openat(ks, proc, path_ptr, flags)) {
                set_syscall_return(ctx, -1);
                rcore_local_context_move_next(ctx);
            }
            break;
        }
        case RCORE_SYS_CLOSE:
            handle_close(proc, args[0]);
            break;
        case RCORE_SYS_WRITE:
            if (!handle_write(ks, proc, args[0], args[1], args[2])) {
                set_syscall_return(ctx, -1);
                rcore_local_context_move_next(ctx);
            }
            break;
        case RCORE_SYS_READ:
            if (!handle_read(ks, proc, args[0], args[1], args[2])) {
                set_syscall_return(ctx, -1);
                rcore_local_context_move_next(ctx);
            }
            break;
        case RCORE_SYS_CLOCK_GETTIME:
            if (!handle_clock_gettime(args[1], ctx)) {
                set_syscall_return(ctx, -1);
                rcore_local_context_move_next(ctx);
            }
            break;
        case RCORE_SYS_SCHED_YIELD:
            handle_sched_yield(proc);
            break;
        case RCORE_SYS_GETPID:
            handle_getpid(proc);
            break;
        case RCORE_SYS_KILL:
            handle_kill(ks, index, (long)args[0], (long)args[1]);
            break;
        case RCORE_SYS_RT_SIGACTION:
            handle_sigaction(proc, (long)args[0], args[1], args[2]);
            break;
        case RCORE_SYS_RT_SIGPROCMASK:
            handle_sigprocmask(proc, (uint64_t)args[0]);
            break;
        case RCORE_SYS_RT_SIGRETURN:
            handle_sigreturn(ks, index);
            break;
        case RCORE_SYS_CLONE:
            handle_fork(ks, index);
            break;
        case RCORE_SYS_EXECVE:
            handle_execve(ks, index, args[0]);
            break;
        case RCORE_SYS_WAIT4:
            handle_wait4(ks, index, (long)args[0], args[1]);
            break;
        case RCORE_SYS_EXIT:
        case RCORE_SYS_EXIT_GROUP:
            handle_exit(ks, index, (long)args[0]);
            break;
        default:
            log_prefix("[ERROR]");
            rcore_console_puts("unsupported syscall ");
            print_unsigned((size_t)id);
            rcore_console_puts("\n");
            finish_process(ks, index, -2);
            break;
    }
}

static void schedule(struct kernel_state *ks) {
    for (;;) {
        int next = pick_next_runnable(ks);
        if (next < 0) {
            asm volatile("wfi");
            continue;
        }

        ks->current_index = (size_t)next;
        ks->has_current = true;
        struct process *proc = &ks->procs[(size_t)next];
        proc->state = PROC_RUNNING;
        restore_store_to_exec(proc);
        if (!prepare_signal_delivery(ks, (size_t)next)) {
            if (proc->used && proc->state == PROC_RUNNING) {
                proc->state = PROC_RUNNABLE;
            }
            if (proc->used) {
                copy_exec_to_store(proc);
            }
            continue;
        }
        if (proc->signal_depth > 0) {
            rcore_program_timer_slice();
        } else {
            rcore_cancel_timer_slice();
        }
        (void)rcore_local_context_execute(&proc->ctx);
        rcore_cancel_timer_slice();

        uint64_t scause = rcore_read_scause();
        bool is_interrupt = (scause >> 63) != 0;
        uint64_t code = scause & ((((uint64_t)1) << 63) - 1);
        if (!is_interrupt && code == 8) {
            handle_syscall(ks, (size_t)next);
        } else if (is_interrupt && code == 5) {
            /* Timer preemption for long-running user handlers and threads. */
        } else {
            log_trap_and_finish(ks, (size_t)next, scause, rcore_read_stval());
        }

        if (proc->used && proc->state == PROC_RUNNING) {
            proc->state = PROC_RUNNABLE;
        }
        if (proc->used) {
            copy_exec_to_store(proc);
        }
    }
}

__attribute__((noreturn)) void rust_main(void) {
    struct rcore_kernel_layout layout = rcore_kernel_layout_locate();
    rcore_zero_bss(&layout);
    init_console();
    set_log_level(NULL);
    test_log();
    rcore_enable_timer_interrupt();
    rcore_equiv_emit("boot", "ch7", "enter", "ok", "lang=c,chapter=7");

    struct kernel_state *ks = kernel_state();
    memset(ks, 0, sizeof(*ks));
    ks->next_pid = 1;
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        clear_process(&ks->procs[i]);
    }
    if (!stage_all_apps(ks)) {
        log_prefix("[ERROR]");
        rcore_console_puts("failed to stage ch7 app catalog\n");
        for (;;) {
            asm volatile("wfi");
        }
    }

    int initproc_index = find_app_index_by_name(ks, "initproc");
    if (initproc_index < 0 || spawn_process_from_app(ks, initproc_index, 0, 1) < 0) {
        log_prefix("[ERROR]");
        rcore_console_puts("failed to spawn initproc\n");
        for (;;) {
            asm volatile("wfi");
        }
    }
    log_prefix("[ INFO]");
    rcore_console_puts("Loaded initproc\n");
    schedule(ks);
    __builtin_unreachable();
}

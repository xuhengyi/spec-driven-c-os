#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rcore/console.h"
#include "rcore/equiv.h"
#include "rcore/kernel_context_local.h"
#include "rcore/linker.h"
#include "rcore/syscalls.h"

#ifndef RCORE_SYS_WAITTID
#define RCORE_SYS_WAITTID 1001
#endif

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDDEBUG_FD 2
#define FIRST_USER_FD 3

#define EXEC_SLOT_BASE 0x82300000UL
#define PROCESS_SLOT_SIZE 0x00080000UL
#define PROCESS_STORE_BASE 0x82400000UL
#define MAX_PROCESS_COUNT 34U
#define KERNEL_STATE_BASE 0x83600000UL
#define APP_STAGE_BASE 0x83700000UL
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
#define MAX_THREAD_COUNT 24U
#define THREAD_STACK_SIZE 0x2000UL
#define MAX_MUTEX_COUNT 8U
#define MAX_SEMAPHORE_COUNT 8U
#define MAX_CONDVAR_COUNT 8U

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

struct thread {
    bool used;
    int tid;
    enum proc_state state;
    int exit_code;
    struct rcore_local_context ctx;
};

struct wait_queue {
    size_t items[MAX_THREAD_COUNT];
    size_t count;
};

struct mutex_object {
    bool used;
    bool blocking;
    bool locked;
    int owner_tid;
    struct wait_queue waiters;
};

struct semaphore_object {
    bool used;
    size_t count;
    struct wait_queue waiters;
};

struct condvar_waiter {
    size_t thread_index;
    size_t mutex_index;
};

struct condvar_object {
    bool used;
    struct condvar_waiter waiters[MAX_THREAD_COUNT];
    size_t count;
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
    int next_tid;
    bool waiting_on_thread;
    int wait_thread_tid;
    struct thread threads[MAX_THREAD_COUNT];
    struct mutex_object mutexes[MAX_MUTEX_COUNT];
    struct semaphore_object semaphores[MAX_SEMAPHORE_COUNT];
    struct condvar_object condvars[MAX_CONDVAR_COUNT];
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
    bool current_is_thread;
    size_t current_thread_index;
    size_t loaded_process_index;
    bool has_loaded_process;
    bool loaded_process_dirty;
    struct staged_app_image apps[MAX_APP_COUNT];
    struct process procs[MAX_PROCESS_COUNT];
    struct file_node files[MAX_FILE_COUNT];
};

static const char *const CH8_APP_NAMES[] = {
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
    "threads",
    "threads_arg",
    "mpsc_sem",
    "sync_sem",
    "race_adder_mutex_blocking",
    "test_condvar",
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
    if (count == 0 || count > MAX_APP_COUNT || count > (sizeof(CH8_APP_NAMES) / sizeof(CH8_APP_NAMES[0]))) {
        log_prefix("[ERROR]");
        rcore_console_puts("stage_all_apps count=");
        print_unsigned(count);
        rcore_console_puts(" max_app=");
        print_unsigned(MAX_APP_COUNT);
        rcore_console_puts(" name_count=");
        print_unsigned(sizeof(CH8_APP_NAMES) / sizeof(CH8_APP_NAMES[0]));
        rcore_console_puts("\n");
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        struct rcore_app_meta meta = rcore_app_get(i);
        if (!rcore_app_meta_valid(&meta) || meta.start == 0 || meta.size == 0) {
            log_prefix("[ERROR]");
            rcore_console_puts("invalid app meta index=");
            print_unsigned(i);
            rcore_console_puts(" start=");
            print_hex(meta.start);
            rcore_console_puts(" size=");
            print_unsigned(meta.size);
            rcore_console_puts("\n");
            return false;
        }
        ks->apps[i].start = meta.start;
        ks->apps[i].size = meta.size;
        copy_c_string(ks->apps[i].name, sizeof(ks->apps[i].name), CH8_APP_NAMES[i]);
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
    int best_index = -1;
    size_t best_len = 0;
    for (size_t i = 0; i < ks->app_count; ++i) {
        const char *name = ks->apps[i].name;
        size_t len = 0;
        while (name[len] != '\0') {
            if (src[len] != name[len]) {
                break;
            }
            ++len;
        }
        if (name[len] == '\0' && len > best_len) {
            best_index = (int)i;
            best_len = len;
        }
    }
    return best_index;
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

static void clear_thread(struct thread *thread) {
    memset(thread, 0, sizeof(*thread));
    thread->state = PROC_UNUSED;
}

static void clear_process(struct process *proc) {
    memset(proc, 0, sizeof(*proc));
    proc->state = PROC_UNUSED;
    proc->wait_pid = -1;
    proc->wait_thread_tid = -1;
    proc->next_tid = 1;
    for (size_t i = 0; i < MAX_THREAD_COUNT; ++i) {
        clear_thread(&proc->threads[i]);
    }
}

static int alloc_thread_index(struct process *proc) {
    for (size_t i = 0; i < MAX_THREAD_COUNT; ++i) {
        if (!proc->threads[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int find_thread_index_by_tid(const struct process *proc, int tid) {
    for (size_t i = 0; i < MAX_THREAD_COUNT; ++i) {
        if (proc->threads[i].used && proc->threads[i].tid == tid) {
            return (int)i;
        }
    }
    return -1;
}

static uintptr_t thread_stack_top(size_t index) {
    return EXEC_SLOT_BASE + PROCESS_SLOT_SIZE - (index + 1) * THREAD_STACK_SIZE;
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

static void flush_loaded_process(struct kernel_state *ks) {
    if (!ks->has_loaded_process) {
        return;
    }
    if (ks->loaded_process_index < MAX_PROCESS_COUNT) {
        struct process *proc = &ks->procs[ks->loaded_process_index];
        if (proc->used && ks->loaded_process_dirty) {
            copy_exec_to_store(proc);
        }
    }
    ks->loaded_process_dirty = false;
}

static void ensure_loaded_process(struct kernel_state *ks, size_t proc_index) {
    if (ks->has_loaded_process && ks->loaded_process_index == proc_index) {
        return;
    }
    flush_loaded_process(ks);
    restore_store_to_exec(&ks->procs[proc_index]);
    ks->loaded_process_index = proc_index;
    ks->has_loaded_process = true;
    ks->loaded_process_dirty = false;
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

static void wait_queue_push(struct wait_queue *queue, size_t value) {
    if (queue->count < MAX_THREAD_COUNT) {
        queue->items[queue->count++] = value;
    }
}

static bool wait_queue_pop(struct wait_queue *queue, size_t *value) {
    if (queue->count == 0) {
        return false;
    }
    *value = queue->items[0];
    for (size_t i = 1; i < queue->count; ++i) {
        queue->items[i - 1] = queue->items[i];
    }
    queue->count--;
    return true;
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

static struct rcore_local_context *active_ctx(struct kernel_state *ks, struct process *proc) {
    if (ks->current_is_thread) {
        return &proc->threads[ks->current_thread_index].ctx;
    }
    return &proc->ctx;
}

static int active_tid(const struct kernel_state *ks, const struct process *proc) {
    if (ks->current_is_thread) {
        return proc->threads[ks->current_thread_index].tid;
    }
    return 0;
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

static void wake_main_thread_for_waittid(struct process *proc, struct thread *thread) {
    if (!proc->waiting_on_thread || proc->state != PROC_WAITING || proc->wait_thread_tid != thread->tid) {
        return;
    }
    set_syscall_return(&proc->ctx, thread->exit_code);
    proc->state = PROC_RUNNABLE;
    proc->waiting_on_thread = false;
    proc->wait_thread_tid = -1;
    clear_thread(thread);
}

static void finish_thread(struct process *proc, size_t thread_index, int exit_code) {
    struct thread *thread = &proc->threads[thread_index];
    thread->exit_code = exit_code;
    thread->state = PROC_ZOMBIE;
    wake_main_thread_for_waittid(proc, thread);
}

static bool process_has_runnable_threads(const struct process *proc) {
    for (size_t i = 0; i < MAX_THREAD_COUNT; ++i) {
        if (proc->threads[i].used && proc->threads[i].state == PROC_RUNNABLE) {
            return true;
        }
    }
    return false;
}

static bool mutex_try_wake_waiter(struct process *proc, struct mutex_object *mutex) {
    size_t thread_index = 0;
    while (wait_queue_pop(&mutex->waiters, &thread_index)) {
        if (thread_index < MAX_THREAD_COUNT && proc->threads[thread_index].used) {
            mutex->locked = true;
            mutex->owner_tid = proc->threads[thread_index].tid;
            proc->threads[thread_index].state = PROC_RUNNABLE;
            return true;
        }
    }
    return false;
}

static void mutex_release(struct process *proc, size_t mutex_id) {
    if (mutex_id >= MAX_MUTEX_COUNT || !proc->mutexes[mutex_id].used) {
        return;
    }
    struct mutex_object *mutex = &proc->mutexes[mutex_id];
    if (!mutex_try_wake_waiter(proc, mutex)) {
        mutex->locked = false;
        mutex->owner_tid = -1;
    }
}

static void reap_process(struct process *proc) {
    clear_process(proc);
}

static void reparent_or_reap_children(struct kernel_state *ks, int parent_pid) {
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        struct process *child = &ks->procs[i];
        if (!child->used || child->parent_pid != parent_pid) {
            continue;
        }
        if (child->state == PROC_ZOMBIE) {
            reap_process(child);
        } else {
            child->parent_pid = 1;
        }
    }
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
    reparent_or_reap_children(ks, proc->pid);
    for (size_t i = 0; i < MAX_THREAD_COUNT; ++i) {
        clear_thread(&proc->threads[i]);
    }
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
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    uint64_t code = scause & ((((uint64_t)1) << 63) - 1);
    log_prefix("[ERROR]");
    rcore_console_puts("Trap Exception(");
    rcore_console_puts(exception_name(code));
    rcore_console_puts(") stval=");
    print_hex((uintptr_t)stval);
    rcore_console_puts(" pc=");
    print_hex(ctx->sepc);
    rcore_console_puts("\n");
    if (ks->current_is_thread) {
        finish_thread(proc, ks->current_thread_index, -3);
    } else {
        finish_process(ks, index, -3);
    }
}

static size_t current_task_slot(const struct kernel_state *ks) {
    if (!ks->has_current) {
        return 0;
    }
    return ks->current_index * (MAX_THREAD_COUNT + 1U)
        + (ks->current_is_thread ? (ks->current_thread_index + 1U) : 0U);
}

static bool decode_runnable_task(
    const struct kernel_state *ks,
    size_t slot,
    size_t *proc_index_out,
    bool *is_thread_out,
    size_t *thread_index_out
) {
    size_t slots_per_process = MAX_THREAD_COUNT + 1U;
    size_t proc_index = slot / slots_per_process;
    size_t subslot = slot % slots_per_process;
    if (proc_index >= MAX_PROCESS_COUNT) {
        return false;
    }
    const struct process *proc = &ks->procs[proc_index];
    if (!proc->used || proc->state == PROC_ZOMBIE) {
        return false;
    }
    if (subslot == 0) {
        if (proc->state != PROC_RUNNABLE) {
            return false;
        }
        *proc_index_out = proc_index;
        *is_thread_out = false;
        *thread_index_out = 0;
        return true;
    }

    size_t thread_index = subslot - 1U;
    if (thread_index >= MAX_THREAD_COUNT || proc->state == PROC_STOPPED) {
        return false;
    }
    const struct thread *thread = &proc->threads[thread_index];
    if (!thread->used || thread->state != PROC_RUNNABLE) {
        return false;
    }
    *proc_index_out = proc_index;
    *is_thread_out = true;
    *thread_index_out = thread_index;
    return true;
}

static bool pick_next_runnable(
    const struct kernel_state *ks,
    size_t *proc_index_out,
    bool *is_thread_out,
    size_t *thread_index_out
) {
    size_t total_slots = MAX_PROCESS_COUNT * (MAX_THREAD_COUNT + 1U);
    size_t start = ks->has_current ? (current_task_slot(ks) + 1U) % total_slots : 0;
    for (size_t offset = 0; offset < total_slots; ++offset) {
        size_t slot = (start + offset) % total_slots;
        if (decode_runnable_task(ks, slot, proc_index_out, is_thread_out, thread_index_out)) {
            return true;
        }
    }
    return false;
}

static bool handle_openat(struct kernel_state *ks, struct process *proc, uintptr_t path_ptr, uintptr_t flags) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
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
    set_syscall_return(ctx, fd);
    rcore_local_context_move_next(ctx);
    return true;
}

static void handle_close(struct kernel_state *ks, struct process *proc, uintptr_t fd) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    if (fd < FIRST_USER_FD) {
        set_syscall_return(ctx, 0);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct fd_entry *entry = lookup_fd(proc, fd);
    if (entry == NULL) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    memset(entry, 0, sizeof(*entry));
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
}

static bool handle_write(struct kernel_state *ks, struct process *proc, uintptr_t fd, uintptr_t buf, uintptr_t count) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
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
    struct rcore_local_context *ctx = active_ctx(ks, proc);
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

static void handle_getpid(struct kernel_state *ks, struct process *proc) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    set_syscall_return(ctx, proc->pid);
    rcore_local_context_move_next(ctx);
}

static void handle_gettid(struct kernel_state *ks, struct process *proc) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    set_syscall_return(ctx, active_tid(ks, proc));
    rcore_local_context_move_next(ctx);
}

static void handle_sched_yield(struct kernel_state *ks, struct process *proc) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
}

static void handle_exit(struct kernel_state *ks, size_t index, long exit_code) {
    struct process *proc = &ks->procs[index];
    if (ks->current_is_thread) {
        finish_thread(proc, ks->current_thread_index, (int)exit_code);
        return;
    }
    finish_process(ks, index, (int)exit_code);
}

static void handle_wait4(struct kernel_state *ks, size_t index, long waited_pid, uintptr_t status_ptr) {
    struct process *proc = &ks->procs[index];
    if (ks->current_is_thread) {
        struct rcore_local_context *ctx = active_ctx(ks, proc);
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
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
    if (ks->current_is_thread) {
        struct rcore_local_context *ctx = active_ctx(ks, parent);
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
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
    if (ks->current_is_thread) {
        struct rcore_local_context *ctx = active_ctx(ks, proc);
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
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
    if (ks->current_is_thread) {
        struct rcore_local_context *ctx = active_ctx(ks, proc);
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    if (proc->signal_depth == 0) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    struct saved_signal_context *frame = &proc->signal_stack[proc->signal_depth - 1];
    proc->ctx = frame->ctx;
    proc->signal_depth--;
}

static void wake_main_waiting_for_thread(struct process *proc, int tid, int exit_code) {
    if (!proc->waiting_on_thread || proc->state != PROC_WAITING || proc->wait_thread_tid != tid) {
        return;
    }
    set_syscall_return(&proc->ctx, exit_code);
    proc->state = PROC_RUNNABLE;
    proc->waiting_on_thread = false;
    proc->wait_thread_tid = -1;
}

static void handle_thread_create(struct kernel_state *ks, struct process *proc, uintptr_t entry, uintptr_t arg) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    int thread_index = alloc_thread_index(proc);
    if (thread_index < 0) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct thread *thread = &proc->threads[thread_index];
    clear_thread(thread);
    thread->used = true;
    thread->tid = proc->next_tid++;
    thread->state = PROC_RUNNABLE;
    thread->ctx = rcore_local_context_user(entry);
    rcore_local_context_set_sp(&thread->ctx, thread_stack_top((size_t)thread_index));
    rcore_local_context_set_a(&thread->ctx, 0, arg);
    set_syscall_return(ctx, thread->tid);
    rcore_local_context_move_next(ctx);
}

static void handle_waittid(struct kernel_state *ks, struct process *proc, uintptr_t tid_value) {
    if (ks->current_is_thread) {
        struct rcore_local_context *ctx = active_ctx(ks, proc);
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    int thread_index = find_thread_index_by_tid(proc, (int)tid_value);
    if (thread_index < 0) {
        set_syscall_return(&proc->ctx, -1);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    struct thread *thread = &proc->threads[thread_index];
    if (thread->state == PROC_ZOMBIE) {
        int exit_code = thread->exit_code;
        clear_thread(thread);
        set_syscall_return(&proc->ctx, exit_code);
        rcore_local_context_move_next(&proc->ctx);
        return;
    }
    rcore_local_context_move_next(&proc->ctx);
    proc->state = PROC_WAITING;
    proc->waiting_on_thread = true;
    proc->wait_thread_tid = (int)tid_value;
}

static void handle_mutex_create(struct kernel_state *ks, struct process *proc, uintptr_t blocking) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    for (size_t i = 0; i < MAX_MUTEX_COUNT; ++i) {
        if (!proc->mutexes[i].used) {
            memset(&proc->mutexes[i], 0, sizeof(proc->mutexes[i]));
            proc->mutexes[i].used = true;
            proc->mutexes[i].blocking = blocking != 0;
            proc->mutexes[i].owner_tid = -1;
            set_syscall_return(ctx, (long)i);
            rcore_local_context_move_next(ctx);
            return;
        }
    }
    set_syscall_return(ctx, -1);
    rcore_local_context_move_next(ctx);
}

static void handle_mutex_lock(struct kernel_state *ks, struct process *proc, uintptr_t mutex_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    int tid = active_tid(ks, proc);
    if (mutex_id >= MAX_MUTEX_COUNT || !proc->mutexes[mutex_id].used) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct mutex_object *mutex = &proc->mutexes[mutex_id];
    if (!mutex->locked) {
        mutex->locked = true;
        mutex->owner_tid = tid;
        set_syscall_return(ctx, 0);
        rcore_local_context_move_next(ctx);
        return;
    }
    if (!mutex->blocking || !ks->current_is_thread) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    wait_queue_push(&mutex->waiters, ks->current_thread_index);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
    proc->threads[ks->current_thread_index].state = PROC_WAITING;
}

static void handle_mutex_unlock(struct kernel_state *ks, struct process *proc, uintptr_t mutex_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    int tid = active_tid(ks, proc);
    if (mutex_id >= MAX_MUTEX_COUNT || !proc->mutexes[mutex_id].used) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct mutex_object *mutex = &proc->mutexes[mutex_id];
    if (!mutex->locked || mutex->owner_tid != tid) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    mutex_release(proc, (size_t)mutex_id);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
}

static void handle_semaphore_create(struct kernel_state *ks, struct process *proc, uintptr_t count) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    for (size_t i = 0; i < MAX_SEMAPHORE_COUNT; ++i) {
        if (!proc->semaphores[i].used) {
            memset(&proc->semaphores[i], 0, sizeof(proc->semaphores[i]));
            proc->semaphores[i].used = true;
            proc->semaphores[i].count = (size_t)count;
            set_syscall_return(ctx, (long)i);
            rcore_local_context_move_next(ctx);
            return;
        }
    }
    set_syscall_return(ctx, -1);
    rcore_local_context_move_next(ctx);
}

static void handle_semaphore_down(struct kernel_state *ks, struct process *proc, uintptr_t sem_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    if (sem_id >= MAX_SEMAPHORE_COUNT || !proc->semaphores[sem_id].used) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct semaphore_object *sem = &proc->semaphores[sem_id];
    if (sem->count > 0) {
        sem->count--;
        set_syscall_return(ctx, 0);
        rcore_local_context_move_next(ctx);
        return;
    }
    if (!ks->current_is_thread) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    wait_queue_push(&sem->waiters, ks->current_thread_index);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
    proc->threads[ks->current_thread_index].state = PROC_WAITING;
}

static void handle_semaphore_up(struct kernel_state *ks, struct process *proc, uintptr_t sem_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    if (sem_id >= MAX_SEMAPHORE_COUNT || !proc->semaphores[sem_id].used) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct semaphore_object *sem = &proc->semaphores[sem_id];
    size_t thread_index = 0;
    if (wait_queue_pop(&sem->waiters, &thread_index) && thread_index < MAX_THREAD_COUNT && proc->threads[thread_index].used) {
        proc->threads[thread_index].state = PROC_RUNNABLE;
    } else {
        sem->count++;
    }
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
}

static void handle_condvar_create(struct kernel_state *ks, struct process *proc) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    for (size_t i = 0; i < MAX_CONDVAR_COUNT; ++i) {
        if (!proc->condvars[i].used) {
            memset(&proc->condvars[i], 0, sizeof(proc->condvars[i]));
            proc->condvars[i].used = true;
            set_syscall_return(ctx, (long)i);
            rcore_local_context_move_next(ctx);
            return;
        }
    }
    set_syscall_return(ctx, -1);
    rcore_local_context_move_next(ctx);
}

static void handle_condvar_wait(struct kernel_state *ks, struct process *proc, uintptr_t cond_id, uintptr_t mutex_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    if (!ks->current_is_thread || cond_id >= MAX_CONDVAR_COUNT || mutex_id >= MAX_MUTEX_COUNT
        || !proc->condvars[cond_id].used || !proc->mutexes[mutex_id].used
        || proc->condvars[cond_id].count >= MAX_THREAD_COUNT) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    if (!proc->mutexes[mutex_id].locked || proc->mutexes[mutex_id].owner_tid != active_tid(ks, proc)) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct condvar_object *cond = &proc->condvars[cond_id];
    cond->waiters[cond->count].thread_index = ks->current_thread_index;
    cond->waiters[cond->count].mutex_index = (size_t)mutex_id;
    cond->count++;
    mutex_release(proc, (size_t)mutex_id);
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
    proc->threads[ks->current_thread_index].state = PROC_WAITING;
}

static void handle_condvar_signal(struct kernel_state *ks, struct process *proc, uintptr_t cond_id) {
    struct rcore_local_context *ctx = active_ctx(ks, proc);
    if (cond_id >= MAX_CONDVAR_COUNT || !proc->condvars[cond_id].used) {
        set_syscall_return(ctx, -1);
        rcore_local_context_move_next(ctx);
        return;
    }
    struct condvar_object *cond = &proc->condvars[cond_id];
    if (cond->count > 0) {
        struct condvar_waiter waiter = cond->waiters[0];
        for (size_t i = 1; i < cond->count; ++i) {
            cond->waiters[i - 1] = cond->waiters[i];
        }
        cond->count--;
        if (waiter.thread_index < MAX_THREAD_COUNT && proc->threads[waiter.thread_index].used) {
            struct mutex_object *mutex = &proc->mutexes[waiter.mutex_index];
            if (!mutex->locked) {
                mutex->locked = true;
                mutex->owner_tid = proc->threads[waiter.thread_index].tid;
                proc->threads[waiter.thread_index].state = PROC_RUNNABLE;
            } else {
                wait_queue_push(&mutex->waiters, waiter.thread_index);
            }
        }
    }
    set_syscall_return(ctx, 0);
    rcore_local_context_move_next(ctx);
}

static void handle_syscall(struct kernel_state *ks, size_t index) {
    struct process *proc = &ks->procs[index];
    struct rcore_local_context *ctx = active_ctx(ks, proc);
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
            handle_close(ks, proc, args[0]);
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
            handle_sched_yield(ks, proc);
            break;
        case RCORE_SYS_GETPID:
            handle_getpid(ks, proc);
            break;
        case RCORE_SYS_GETTID:
            handle_gettid(ks, proc);
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
        case RCORE_SYS_WAITID:
            handle_waittid(ks, proc, args[0]);
            break;
        case RCORE_SYS_THREAD_CREATE:
            handle_thread_create(ks, proc, args[0], args[1]);
            break;
        case RCORE_SYS_WAITTID:
            handle_waittid(ks, proc, args[0]);
            break;
        case RCORE_SYS_MUTEX_CREATE:
            handle_mutex_create(ks, proc, args[0]);
            break;
        case RCORE_SYS_MUTEX_LOCK:
            handle_mutex_lock(ks, proc, args[0]);
            break;
        case RCORE_SYS_MUTEX_UNLOCK:
            handle_mutex_unlock(ks, proc, args[0]);
            break;
        case RCORE_SYS_SEMAPHORE_CREATE:
            handle_semaphore_create(ks, proc, args[0]);
            break;
        case RCORE_SYS_SEMAPHORE_DOWN:
            handle_semaphore_down(ks, proc, args[0]);
            break;
        case RCORE_SYS_SEMAPHORE_UP:
            handle_semaphore_up(ks, proc, args[0]);
            break;
        case RCORE_SYS_CONDVAR_CREATE:
            handle_condvar_create(ks, proc);
            break;
        case RCORE_SYS_CONDVAR_WAIT:
            handle_condvar_wait(ks, proc, args[0], args[1]);
            break;
        case RCORE_SYS_CONDVAR_SIGNAL:
            handle_condvar_signal(ks, proc, args[0]);
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
            if (ks->current_is_thread) {
                finish_thread(proc, ks->current_thread_index, -2);
            } else {
                finish_process(ks, index, -2);
            }
            break;
    }
}

static void schedule(struct kernel_state *ks) {
    for (;;) {
        size_t next = 0;
        bool next_is_thread = false;
        size_t next_thread_index = 0;
        if (!pick_next_runnable(ks, &next, &next_is_thread, &next_thread_index)) {
            flush_loaded_process(ks);
            asm volatile("wfi");
            continue;
        }

        ks->current_index = next;
        ks->has_current = true;
        ks->current_is_thread = next_is_thread;
        ks->current_thread_index = next_thread_index;
        struct process *proc = &ks->procs[next];
        struct rcore_local_context *ctx = active_ctx(ks, proc);
        if (ks->current_is_thread) {
            proc->threads[ks->current_thread_index].state = PROC_RUNNING;
        } else {
            proc->state = PROC_RUNNING;
        }
        ensure_loaded_process(ks, next);
        if (!ks->current_is_thread && !prepare_signal_delivery(ks, next)) {
            if (proc->used && proc->state == PROC_RUNNING) {
                proc->state = PROC_RUNNABLE;
            }
            ks->loaded_process_dirty = true;
            continue;
        }
        if (proc->signal_depth > 0 || ks->current_is_thread) {
            rcore_program_timer_slice();
        } else {
            rcore_cancel_timer_slice();
        }
        (void)rcore_local_context_execute(ctx);
        rcore_cancel_timer_slice();

        uint64_t scause = rcore_read_scause();
        bool is_interrupt = (scause >> 63) != 0;
        uint64_t code = scause & ((((uint64_t)1) << 63) - 1);
        if (!is_interrupt && code == 8) {
            handle_syscall(ks, next);
        } else if (is_interrupt && code == 5) {
            /* Timer preemption for long-running user handlers and threads. */
        } else {
            log_trap_and_finish(ks, next, scause, rcore_read_stval());
        }

        if (proc->used) {
            if (ks->current_is_thread) {
                if (proc->threads[ks->current_thread_index].used
                    && proc->threads[ks->current_thread_index].state == PROC_RUNNING) {
                    proc->threads[ks->current_thread_index].state = PROC_RUNNABLE;
                }
            } else if (proc->state == PROC_RUNNING) {
                proc->state = PROC_RUNNABLE;
            }
            if (ks->has_loaded_process && ks->loaded_process_index == next) {
                ks->loaded_process_dirty = true;
            }
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
    rcore_equiv_emit("boot", "ch8", "enter", "ok", "lang=c,chapter=8");

    struct kernel_state *ks = kernel_state();
    memset(ks, 0, sizeof(*ks));
    ks->next_pid = 1;
    for (size_t i = 0; i < MAX_PROCESS_COUNT; ++i) {
        clear_process(&ks->procs[i]);
    }
    if (!stage_all_apps(ks)) {
        log_prefix("[ERROR]");
        rcore_console_puts("failed to stage ch8 app catalog\n");
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

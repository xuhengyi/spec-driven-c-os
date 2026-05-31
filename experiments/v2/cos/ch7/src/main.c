// COS ch7 RISC-V OS snapshot generated from bundles/ch7.bundle.md.
// Freestanding supervisor-mode kernel with app images, page tables, files, pipes, and signals.

#include <stddef.h>
#include <stdint.h>

#define SBI_CONSOLE_PUTCHAR 1UL
#define SBI_SHUTDOWN 8UL

#define SYS_OPENAT 56UL
#define SYS_CLOSE 57UL
#define SYS_PIPE2 59UL
#define SYS_READ 63UL
#define SYS_WRITE 64UL
#define SYS_EXIT 93UL
#define SYS_CLOCK_GETTIME 113UL
#define SYS_SCHED_YIELD 124UL
#define SYS_KILL 129UL
#define SYS_RT_SIGACTION 134UL
#define SYS_RT_SIGPROCMASK 135UL
#define SYS_RT_SIGRETURN 139UL
#define SYS_GETPID 172UL
#define SYS_BRK 214UL
#define SYS_CLONE 220UL
#define SYS_EXECVE 221UL
#define SYS_WAIT4 260UL

#define SCAUSE_ILLEGAL_INSTRUCTION 2UL
#define SCAUSE_LOAD_ACCESS_FAULT 5UL
#define SCAUSE_STORE_ACCESS_FAULT 7UL
#define SCAUSE_U_ECALL 8UL
#define SCAUSE_LOAD_PAGE_FAULT 13UL
#define SCAUSE_STORE_PAGE_FAULT 15UL

#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SPP (1UL << 8)
#define SSTATUS_SUM (1UL << 18)
#define SCAUSE_INTERRUPT (1ULL << 63)

#define MAX_TASKS 96UL
#define MAX_FD 16UL
#define MAX_FILES 16UL
#define MAX_PIPES 16UL
#define MAX_SIG 32UL
#define FILE_NAME_SIZE 32UL
#define FILE_DATA_SIZE 4096UL
#define PIPE_DATA_SIZE 4096UL
#define APP_LOAD_LIMIT 0x200000UL
#define USER_STACK_SIZE 16384UL
#define PAGE_SIZE 4096UL
#define PROGRAM_STATIC_SIZE 0x80000UL
#define USER_BASE 0x86000000UL
#define PROCESS_BASE 0x90000000UL
#define KERNEL_MAP_START 0x80000000UL
#define KERNEL_MAP_END 0xA0000000UL
#define SV39_MODE (8ULL << 60)
#define PTE_V (1ULL << 0)
#define PTE_R (1ULL << 1)
#define PTE_W (1ULL << 2)
#define PTE_X (1ULL << 3)
#define PTE_U (1ULL << 4)
#define PTE_A (1ULL << 6)
#define PTE_D (1ULL << 7)
#define TIMEBASE_HZ 10000000ULL
#define NSEC_PER_SEC 1000000000ULL

#define FD_UNUSED 0UL
#define FD_STDIN 1UL
#define FD_STDOUT 2UL
#define FD_FILE 3UL
#define FD_PIPE_READ 4UL
#define FD_PIPE_WRITE 5UL

#define OPEN_WRONLY (1UL << 0)
#define OPEN_RDWR (1UL << 1)
#define OPEN_CREATE (1UL << 9)
#define OPEN_TRUNC (1UL << 10)

#define SIGKILL 9UL
#define SIGUSR1 10UL

struct trap_frame {
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
};

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_EXITED,
    TASK_FAULTED,
};

struct fd_entry {
    uint64_t kind;
    uint64_t file_index;
    uint64_t offset;
    uint64_t readable;
    uint64_t writable;
};

struct file_entry {
    uint64_t used;
    uint64_t len;
    uint64_t name_len;
    uint8_t name[FILE_NAME_SIZE];
    uint8_t data[FILE_DATA_SIZE];
};

struct pipe_entry {
    uint64_t used;
    uint64_t read_open;
    uint64_t write_open;
    uint64_t read_offset;
    uint64_t len;
    uint8_t data[PIPE_DATA_SIZE];
};

struct signal_action {
    uint64_t handler;
    uint64_t mask;
};

struct task {
    struct trap_frame tf;
    enum task_state state;
    uintptr_t pid;
    uintptr_t parent_pid;
    uintptr_t app_base;
    uintptr_t phys_base;
    uintptr_t program_end;
    uintptr_t heap_base;
    uintptr_t brk;
    uintptr_t heap_limit;
    uintptr_t stack_base;
    uintptr_t stack_top;
    int64_t exit_code;
    struct fd_entry fds[MAX_FD];
    struct signal_action signal_actions[MAX_SIG];
    uint64_t signal_mask;
    uint64_t signal_pending;
    uint64_t handling_signal;
    struct trap_frame saved_signal_tf;
};

static const char *app_names[] = {
    "00hello_world",
    "01store_fault",
    "02power",
    "03priv_inst",
    "04priv_csr",
    "05write_a",
    "06write_b",
    "07write_c",
    "08power_3",
    "09power_5",
    "10power_7",
    "12forktest",
    "13forktree",
    "14forktest2",
    "15matrix",
    "fork_exit",
    "forktest_simple",
    "sbrk",
    "filetest_simple",
    "cat_filea",
    "sig_simple",
    "sig_simple2",
    "sig_ctrlc",
    "sig_tests",
    "pipetest",
    "pipe_large_test",
    "ch7b_usertest",
    "user_shell",
    "initproc",
};

#define INITPROC_APP 28UL

extern void __trap_entry(void);
extern void __return_from_user(void);
extern int64_t enter_task(struct trap_frame *tf);

extern uint64_t saved_kernel_sp;
extern uint64_t saved_kernel_ra;

extern uintptr_t apps[];

extern char user_stack0_top[];
extern char user_stack1_top[];
extern char user_stack2_top[];
extern char user_stack3_top[];
extern char user_stack4_top[];
extern char user_stack5_top[];
extern char user_stack6_top[];
extern char user_stack7_top[];
extern char user_stack8_top[];
extern char user_stack9_top[];
extern char user_stack10_top[];
extern char user_stack11_top[];
extern char user_stack12_top[];
extern char user_stack13_top[];
extern char user_stack14_top[];
extern char user_stack15_top[];

static struct task tasks[MAX_TASKS];
static struct file_entry files[MAX_FILES];
static struct pipe_entry pipes[MAX_PIPES];
static size_t task_total;
static size_t live_tasks;
static int current_task = -1;
static uint64_t boot_ticks;
static uint64_t logical_nsec;

static uint64_t root_tables[MAX_TASKS][512] __attribute__((aligned(4096)));
static uint64_t l1_tables[MAX_TASKS][512] __attribute__((aligned(4096)));
static uint64_t user_l0_tables[MAX_TASKS][512] __attribute__((aligned(4096)));

static uint64_t initial_user_sstatus(void);

static inline uintptr_t sbi_call(uintptr_t which, uintptr_t arg0)
{
    register uintptr_t a0 __asm__("a0") = arg0;
    register uintptr_t a7 __asm__("a7") = which;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline uint64_t read_time_counter(void)
{
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static void *k_memset(void *dst, int value, size_t len)
{
    uint8_t *p = (uint8_t *)dst;
    for (size_t i = 0; i < len; i++) {
        p[i] = (uint8_t)value;
    }
    return dst;
}

static void *k_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return dst;
}

static void sbi_putchar(char ch)
{
    (void)sbi_call(SBI_CONSOLE_PUTCHAR, (uintptr_t)ch);
}

static void sbi_shutdown(void)
{
    (void)sbi_call(SBI_SHUTDOWN, 0);
    for (;;) {
        __asm__ volatile("wfi");
    }
}

static size_t app_count(void)
{
    return apps[2] < MAX_TASKS ? (size_t)apps[2] : (size_t)MAX_TASKS;
}

static uintptr_t align_down(uintptr_t value, uintptr_t align)
{
    return value & ~(align - 1);
}

static uintptr_t align_up(uintptr_t value, uintptr_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static size_t vpn2(uintptr_t va)
{
    return (va >> 30) & 0x1ffUL;
}

static size_t vpn1(uintptr_t va)
{
    return (va >> 21) & 0x1ffUL;
}

static size_t vpn0(uintptr_t va)
{
    return (va >> 12) & 0x1ffUL;
}

static uint64_t pte(uintptr_t pa, uint64_t flags)
{
    return ((pa >> 12) << 10) | flags | PTE_V;
}

static void flush_tlb(void)
{
    __asm__ volatile("sfence.vma" ::: "memory");
}

static void disable_paging(void)
{
    __asm__ volatile("csrw satp, zero" ::: "memory");
    flush_tlb();
}

static void map_user_region(
    size_t index,
    uintptr_t start,
    uintptr_t end,
    uintptr_t phys_start,
    uint64_t flags)
{
    uintptr_t va = align_down(start, PAGE_SIZE);
    uintptr_t limit = align_up(end, PAGE_SIZE);
    uintptr_t pa = align_down(phys_start, PAGE_SIZE);
    while (va < limit) {
        user_l0_tables[index][vpn0(va)] = pte(pa, flags);
        va += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
}

static void map_user_heap(size_t index, uintptr_t start, uintptr_t end)
{
    uintptr_t phys_base = tasks[index].phys_base;
    map_user_region(
        index,
        align_down(start, PAGE_SIZE),
        align_up(end, PAGE_SIZE),
        phys_base + (align_down(start, PAGE_SIZE) - USER_BASE),
        PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
}

static void unmap_user_heap(size_t index, uintptr_t start, uintptr_t end)
{
    uintptr_t va = align_up(start, PAGE_SIZE);
    uintptr_t limit = align_up(end, PAGE_SIZE);
    while (va < limit) {
        user_l0_tables[index][vpn0(va)] = 0;
        va += PAGE_SIZE;
    }
}

static void setup_address_space(
    size_t index,
    uintptr_t base,
    uintptr_t program_end,
    uintptr_t stack_base,
    uintptr_t stack_top,
    uintptr_t phys_base)
{
    k_memset(root_tables[index], 0, sizeof(root_tables[index]));
    k_memset(l1_tables[index], 0, sizeof(l1_tables[index]));
    k_memset(user_l0_tables[index], 0, sizeof(user_l0_tables[index]));

    root_tables[index][vpn2(KERNEL_MAP_START)] = pte((uintptr_t)l1_tables[index], 0);
    for (uintptr_t pa = KERNEL_MAP_START; pa < KERNEL_MAP_END; pa += APP_LOAD_LIMIT) {
        if (vpn1(pa) != vpn1(USER_BASE)) {
            l1_tables[index][vpn1(pa)] = pte(pa, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
    }
    l1_tables[index][vpn1(base)] = pte((uintptr_t)user_l0_tables[index], 0);

    map_user_region(index, base, program_end, phys_base, PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D);
    map_user_region(
        index,
        stack_base,
        stack_top,
        phys_base + (stack_base - USER_BASE),
        PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
}

static void activate_task(size_t index)
{
    uint64_t satp = SV39_MODE | (((uintptr_t)root_tables[index]) >> 12);
    __asm__ volatile("csrw satp, %0" :: "r"(satp) : "memory");
    flush_tlb();
}

static int range_contains(uintptr_t start, uintptr_t end, uintptr_t ptr, uint64_t len)
{
    uintptr_t limit = ptr + (uintptr_t)len;
    if (limit < ptr) {
        return 0;
    }
    return ptr >= start && limit <= end;
}

static int user_range_ok(uintptr_t ptr, uint64_t len)
{
    if (len == 0) {
        return 1;
    }
    if (current_task < 0 || (size_t)current_task >= task_total) {
        return 0;
    }

    struct task *task = &tasks[current_task];
    return range_contains(task->app_base, task->program_end, ptr, len) ||
           range_contains(task->heap_base, task->brk, ptr, len) ||
           range_contains(task->stack_base, task->stack_top, ptr, len);
}

static int64_t sys_write(uint64_t fd, uintptr_t buf, uint64_t len)
{
    if (!user_range_ok(buf, len)) {
        return -1;
    }

    if (fd == 1 || fd == 2) {
        const volatile char *chars = (const volatile char *)buf;
        for (uint64_t i = 0; i < len; i++) {
            sbi_putchar(chars[i]);
        }
        return (int64_t)len;
    }

    if (current_task < 0 || fd >= MAX_FD) {
        return -1;
    }
    struct fd_entry *entry = &tasks[current_task].fds[fd];
    if (entry->kind == FD_PIPE_WRITE) {
        if (entry->file_index >= MAX_PIPES || pipes[entry->file_index].used == 0) {
            return -1;
        }
        struct pipe_entry *pipe = &pipes[entry->file_index];
        if (pipe->read_open == 0) {
            return -1;
        }
        uint64_t available = PIPE_DATA_SIZE - pipe->len;
        if (available == 0) {
            return -2;
        }
        uint64_t write_len = len < available ? len : available;
        const volatile uint8_t *src = (const volatile uint8_t *)buf;
        for (uint64_t i = 0; i < write_len; i++) {
            pipe->data[pipe->len + i] = src[i];
        }
        pipe->len += write_len;
        return (int64_t)write_len;
    }
    if (entry->kind != FD_FILE || entry->writable == 0 || entry->file_index >= MAX_FILES) {
        return -1;
    }

    struct file_entry *file = &files[entry->file_index];
    const volatile uint8_t *src = (const volatile uint8_t *)buf;
    uint64_t written = 0;
    while (written < len && entry->offset < FILE_DATA_SIZE) {
        file->data[entry->offset] = src[written];
        entry->offset++;
        written++;
    }
    if (entry->offset > file->len) {
        file->len = entry->offset;
    }
    return (int64_t)written;
}

static int64_t sys_read(uint64_t fd, uintptr_t buf, uint64_t len)
{
    if (!user_range_ok(buf, len)) {
        return -1;
    }
    if (fd == 0) {
        return 0;
    }
    if (current_task < 0 || fd >= MAX_FD) {
        return -1;
    }

    struct fd_entry *entry = &tasks[current_task].fds[fd];
    if (entry->kind == FD_PIPE_READ) {
        if (entry->file_index >= MAX_PIPES || pipes[entry->file_index].used == 0) {
            return -1;
        }
        struct pipe_entry *pipe = &pipes[entry->file_index];
        uint64_t available = pipe->len - pipe->read_offset;
        if (available == 0) {
            return pipe->write_open == 0 ? 0 : -2;
        }
        uint64_t read_len = len < available ? len : available;
        volatile uint8_t *dst = (volatile uint8_t *)buf;
        for (uint64_t i = 0; i < read_len; i++) {
            dst[i] = pipe->data[pipe->read_offset + i];
        }
        pipe->read_offset += read_len;
        if (pipe->read_offset == pipe->len) {
            pipe->read_offset = 0;
            pipe->len = 0;
        }
        return (int64_t)read_len;
    }
    if (entry->kind != FD_FILE || entry->readable == 0 || entry->file_index >= MAX_FILES) {
        return -1;
    }

    struct file_entry *file = &files[entry->file_index];
    volatile uint8_t *dst = (volatile uint8_t *)buf;
    uint64_t read_len = 0;
    while (read_len < len && entry->offset < file->len) {
        dst[read_len] = file->data[entry->offset];
        entry->offset++;
        read_len++;
    }
    return (int64_t)read_len;
}

static int read_user_cstr(uintptr_t ptr, uint8_t *out, size_t *out_len)
{
    if (ptr == 0) {
        return 0;
    }
    for (size_t i = 0; i < FILE_NAME_SIZE; i++) {
        if (!user_range_ok(ptr + i, 1)) {
            return 0;
        }
        uint8_t byte = ((const volatile uint8_t *)ptr)[i];
        if (byte == 0) {
            *out_len = i;
            return 1;
        }
        out[i] = byte;
    }
    return 0;
}

static int file_name_equal(size_t file_index, const uint8_t *name, size_t name_len)
{
    if (files[file_index].used == 0 || files[file_index].name_len != name_len) {
        return 0;
    }
    for (size_t i = 0; i < name_len; i++) {
        if (files[file_index].name[i] != name[i]) {
            return 0;
        }
    }
    return 1;
}

static int find_file(const uint8_t *name, size_t name_len)
{
    for (size_t i = 0; i < MAX_FILES; i++) {
        if (file_name_equal(i, name, name_len)) {
            return (int)i;
        }
    }
    return -1;
}

static int open_or_create_file(const uint8_t *name, size_t name_len, uint64_t flags)
{
    int existing = find_file(name, name_len);
    if (existing >= 0) {
        if ((flags & OPEN_TRUNC) != 0 && (flags & (OPEN_WRONLY | OPEN_RDWR)) != 0) {
            files[existing].len = 0;
        }
        return existing;
    }

    if ((flags & OPEN_CREATE) == 0) {
        return -1;
    }

    for (size_t i = 0; i < MAX_FILES; i++) {
        if (files[i].used == 0) {
            files[i].used = 1;
            files[i].len = 0;
            files[i].name_len = name_len;
            k_memset(files[i].name, 0, sizeof(files[i].name));
            k_memcpy(files[i].name, name, name_len);
            return (int)i;
        }
    }
    return -1;
}

static int alloc_fd(void)
{
    if (current_task < 0) {
        return -1;
    }
    for (size_t fd = 3; fd < MAX_FD; fd++) {
        if (tasks[current_task].fds[fd].kind == FD_UNUSED) {
            return (int)fd;
        }
    }
    return -1;
}

static int64_t sys_open(uintptr_t path, uint64_t flags)
{
    uint8_t name[FILE_NAME_SIZE];
    size_t name_len = 0;
    k_memset(name, 0, sizeof(name));
    if (!read_user_cstr(path, name, &name_len) || name_len == 0) {
        return -1;
    }

    int file = open_or_create_file(name, name_len, flags);
    if (file < 0) {
        return -1;
    }
    int fd = alloc_fd();
    if (fd < 0) {
        return -1;
    }

    tasks[current_task].fds[fd].kind = FD_FILE;
    tasks[current_task].fds[fd].file_index = (uint64_t)file;
    tasks[current_task].fds[fd].offset = 0;
    tasks[current_task].fds[fd].readable =
        ((flags & OPEN_WRONLY) == 0 || (flags & OPEN_RDWR) != 0) ? 1 : 0;
    tasks[current_task].fds[fd].writable =
        ((flags & (OPEN_WRONLY | OPEN_RDWR)) != 0) ? 1 : 0;
    return fd;
}

static int64_t sys_close(uint64_t fd)
{
    if (current_task < 0 || fd >= MAX_FD || tasks[current_task].fds[fd].kind == FD_UNUSED) {
        return -1;
    }
    struct fd_entry entry = tasks[current_task].fds[fd];
    if ((entry.kind == FD_PIPE_READ || entry.kind == FD_PIPE_WRITE) && entry.file_index < MAX_PIPES) {
        struct pipe_entry *pipe = &pipes[entry.file_index];
        if (entry.kind == FD_PIPE_READ && pipe->read_open > 0) {
            pipe->read_open--;
        }
        if (entry.kind == FD_PIPE_WRITE && pipe->write_open > 0) {
            pipe->write_open--;
        }
        if (pipe->read_open == 0 && pipe->write_open == 0) {
            k_memset(pipe, 0, sizeof(*pipe));
        }
    }
    k_memset(&tasks[current_task].fds[fd], 0, sizeof(tasks[current_task].fds[fd]));
    return 0;
}

static void init_standard_fds(size_t index)
{
    k_memset(tasks[index].fds, 0, sizeof(tasks[index].fds));
    tasks[index].fds[0].kind = FD_STDIN;
    tasks[index].fds[0].readable = 1;
    tasks[index].fds[1].kind = FD_STDOUT;
    tasks[index].fds[1].writable = 1;
    tasks[index].fds[2] = tasks[index].fds[1];
}

static int alloc_pipe(void)
{
    for (size_t i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].used == 0) {
            k_memset(&pipes[i], 0, sizeof(pipes[i]));
            pipes[i].used = 1;
            pipes[i].read_open = 1;
            pipes[i].write_open = 1;
            return (int)i;
        }
    }
    return -1;
}

static int64_t sys_pipe(uintptr_t pipe_fd_ptr)
{
    if (!user_range_ok(pipe_fd_ptr, 2 * sizeof(uintptr_t))) {
        return -1;
    }
    int pipe = alloc_pipe();
    if (pipe < 0) {
        return -1;
    }
    int read_fd = alloc_fd();
    if (read_fd < 0) {
        k_memset(&pipes[pipe], 0, sizeof(pipes[pipe]));
        return -1;
    }
    tasks[current_task].fds[read_fd].kind = FD_PIPE_READ;
    tasks[current_task].fds[read_fd].file_index = (uint64_t)pipe;
    tasks[current_task].fds[read_fd].readable = 1;

    int write_fd = alloc_fd();
    if (write_fd < 0) {
        k_memset(&tasks[current_task].fds[read_fd], 0, sizeof(tasks[current_task].fds[read_fd]));
        k_memset(&pipes[pipe], 0, sizeof(pipes[pipe]));
        return -1;
    }
    tasks[current_task].fds[write_fd].kind = FD_PIPE_WRITE;
    tasks[current_task].fds[write_fd].file_index = (uint64_t)pipe;
    tasks[current_task].fds[write_fd].writable = 1;

    volatile uintptr_t *out = (volatile uintptr_t *)pipe_fd_ptr;
    out[0] = (uintptr_t)read_fd;
    out[1] = (uintptr_t)write_fd;
    return 0;
}

static void clone_fd_refs(size_t index)
{
    for (size_t fd = 0; fd < MAX_FD; fd++) {
        struct fd_entry entry = tasks[index].fds[fd];
        if (entry.kind == FD_PIPE_READ && entry.file_index < MAX_PIPES) {
            pipes[entry.file_index].read_open++;
        } else if (entry.kind == FD_PIPE_WRITE && entry.file_index < MAX_PIPES) {
            pipes[entry.file_index].write_open++;
        }
    }
}

static void close_all_fds(size_t index)
{
    int saved_current = current_task;
    current_task = (int)index;
    for (size_t fd = 0; fd < MAX_FD; fd++) {
        if (tasks[index].fds[fd].kind != FD_UNUSED) {
            (void)sys_close(fd);
        }
    }
    current_task = saved_current;
}

static void init_signal_state(size_t index)
{
    k_memset(tasks[index].signal_actions, 0, sizeof(tasks[index].signal_actions));
    tasks[index].signal_mask = 0;
    tasks[index].signal_pending = 0;
    tasks[index].handling_signal = 0;
    k_memset(&tasks[index].saved_signal_tf, 0, sizeof(tasks[index].saved_signal_tf));
}

static int64_t sys_sigaction(uint64_t signum, uintptr_t action_ptr, uintptr_t old_action_ptr)
{
    if (signum == 0 || signum >= MAX_SIG || signum == SIGKILL) {
        return -1;
    }

    struct signal_action old = tasks[current_task].signal_actions[signum];
    if (old_action_ptr != 0) {
        if (!user_range_ok(old_action_ptr, 2 * sizeof(uintptr_t))) {
            return -1;
        }
        volatile uintptr_t *old_out = (volatile uintptr_t *)old_action_ptr;
        old_out[0] = (uintptr_t)old.handler;
        old_out[1] = (uintptr_t)old.mask;
    }
    if (action_ptr != 0) {
        if (!user_range_ok(action_ptr, 2 * sizeof(uintptr_t))) {
            return -1;
        }
        const volatile uintptr_t *in = (const volatile uintptr_t *)action_ptr;
        tasks[current_task].signal_actions[signum].handler = in[0];
        tasks[current_task].signal_actions[signum].mask = in[1];
    }
    return 0;
}

static int64_t sys_sigprocmask(uint64_t mask)
{
    tasks[current_task].signal_mask = mask;
    return 0;
}

static int64_t sys_kill(uint64_t pid, uint64_t signum)
{
    if (signum == 0 || signum >= MAX_SIG) {
        return -1;
    }
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].pid != pid) {
            continue;
        }
        if (signum == SIGKILL) {
            tasks[i].exit_code = -1;
            tasks[i].state = TASK_EXITED;
            if (live_tasks > 0) {
                live_tasks--;
            }
        } else {
            tasks[i].signal_pending |= 1ULL << signum;
        }
        return 0;
    }
    return -1;
}

static void deliver_pending_signal(struct trap_frame *tf)
{
    if (current_task < 0 || tasks[current_task].handling_signal != 0) {
        return;
    }
    uint64_t pending = tasks[current_task].signal_pending & ~tasks[current_task].signal_mask;
    for (uint64_t signum = 1; signum < MAX_SIG; signum++) {
        if ((pending & (1ULL << signum)) == 0) {
            continue;
        }
        struct signal_action action = tasks[current_task].signal_actions[signum];
        tasks[current_task].signal_pending &= ~(1ULL << signum);
        if (action.handler == 0) {
            if (signum == SIGUSR1) {
                return;
            }
            continue;
        }
        k_memcpy(&tasks[current_task].saved_signal_tf, tf, sizeof(*tf));
        tasks[current_task].handling_signal = signum;
        tasks[current_task].signal_mask |= action.mask;
        tf->sepc = action.handler;
        return;
    }
}

static void sys_sigreturn(struct trap_frame *tf)
{
    k_memcpy(tf, &tasks[current_task].saved_signal_tf, sizeof(*tf));
    k_memcpy(&tasks[current_task].tf, tf, sizeof(*tf));
    tasks[current_task].handling_signal = 0;
}

static uint64_t monotonic_nsec(void)
{
    uint64_t ticks = read_time_counter() - boot_ticks;
    uint64_t real_nsec = (ticks / TIMEBASE_HZ) * NSEC_PER_SEC;
    real_nsec += ((ticks % TIMEBASE_HZ) * NSEC_PER_SEC) / TIMEBASE_HZ;

    if (real_nsec <= logical_nsec) {
        logical_nsec += 1000000ULL;
    } else {
        logical_nsec = real_nsec;
    }
    return logical_nsec;
}

static int64_t sys_clock_gettime(uint64_t clock_id, uintptr_t timespec_ptr)
{
    if ((clock_id != 0 && clock_id != 1) || !user_range_ok(timespec_ptr, 16)) {
        return -1;
    }

    uint64_t now = monotonic_nsec();
    volatile uint64_t *timespec = (volatile uint64_t *)timespec_ptr;
    timespec[0] = now / NSEC_PER_SEC;
    timespec[1] = now % NSEC_PER_SEC;
    return 0;
}

static int64_t sys_sbrk(int64_t delta)
{
    if (current_task < 0 || (size_t)current_task >= task_total) {
        return -1;
    }

    struct task *task = &tasks[current_task];
    uintptr_t old = task->brk;
    if (delta == 0) {
        return (int64_t)old;
    }

    uintptr_t next;
    if (delta > 0) {
        next = old + (uintptr_t)delta;
        if (next < old) {
            return -1;
        }
    } else {
        uintptr_t magnitude = (uintptr_t)(-delta);
        if (magnitude > old) {
            return -1;
        }
        next = old - magnitude;
    }

    if (next < task->heap_base || next > task->heap_limit) {
        return -1;
    }

    disable_paging();
    if (next > old) {
        map_user_heap((size_t)current_task, old, next);
    } else {
        unmap_user_heap((size_t)current_task, next, old);
    }
    task->brk = next;
    activate_task((size_t)current_task);
    return (int64_t)old;
}

static uintptr_t process_phys_base(size_t index)
{
    return PROCESS_BASE + index * APP_LOAD_LIMIT;
}

static int alloc_task_slot(void)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            return (int)i;
        }
    }
    return -1;
}

static int name_equals_user(uintptr_t ptr, uint64_t len, const char *name)
{
    size_t n = 0;
    while (name[n] != '\0') {
        n++;
    }
    if (len != n || !user_range_ok(ptr, len)) {
        return 0;
    }
    const volatile char *user = (const volatile char *)ptr;
    for (size_t i = 0; i < n; i++) {
        if (user[i] != name[i]) {
            return 0;
        }
    }
    return 1;
}

static int find_app_by_user_path(uintptr_t ptr, uint64_t len)
{
    if (len == 0 || len > 64) {
        return -1;
    }
    for (size_t i = 0; i < sizeof(app_names) / sizeof(app_names[0]); i++) {
        if (name_equals_user(ptr, len, app_names[i])) {
            return (int)i;
        }
    }
    return -1;
}

static void load_app_image(size_t app_index, uintptr_t phys_base)
{
    uintptr_t start = apps[3 + app_index];
    uintptr_t end = apps[3 + app_index + 1];
    uintptr_t len = end - start;
    if (len > APP_LOAD_LIMIT) {
        sbi_shutdown();
    }
    k_memset((void *)phys_base, 0, APP_LOAD_LIMIT);
    k_memcpy((void *)phys_base, (const void *)start, len);
    __asm__ volatile("fence.i" ::: "memory");
}

static void reset_task_image(size_t index, size_t app_index)
{
    enum task_state old_state = tasks[index].state;
    uintptr_t old_pid = tasks[index].pid;
    uintptr_t old_parent = tasks[index].parent_pid;
    uintptr_t phys = process_phys_base(index);

    disable_paging();
    load_app_image(app_index, phys);

    uintptr_t program_end = align_up(USER_BASE + PROGRAM_STATIC_SIZE, PAGE_SIZE);
    uintptr_t stack_top = USER_BASE + APP_LOAD_LIMIT;
    uintptr_t stack_base = stack_top - USER_STACK_SIZE;
    uintptr_t heap_base = program_end;
    uintptr_t heap_limit = stack_base;
    setup_address_space(index, USER_BASE, program_end, stack_base, stack_top, phys);

    k_memset(&tasks[index].tf, 0, sizeof(tasks[index].tf));
    tasks[index].tf.sp = stack_top;
    tasks[index].tf.sepc = USER_BASE;
    tasks[index].tf.sstatus = initial_user_sstatus();
    tasks[index].state = old_state;
    tasks[index].exit_code = 0;
    tasks[index].pid = old_pid == 0 ? index + 1 : old_pid;
    tasks[index].parent_pid = old_parent;
    tasks[index].app_base = USER_BASE;
    tasks[index].phys_base = phys;
    tasks[index].program_end = program_end;
    tasks[index].heap_base = heap_base;
    tasks[index].brk = heap_base;
    tasks[index].heap_limit = heap_limit;
    tasks[index].stack_base = stack_base;
    tasks[index].stack_top = stack_top;

    if (old_state != TASK_UNUSED && current_task == (int)index) {
        activate_task(index);
    }
}

static int64_t sys_fork(struct trap_frame *tf)
{
    if (current_task < 0) {
        return -1;
    }
    int child = alloc_task_slot();
    if (child < 0) {
        return -1;
    }

    size_t parent = (size_t)current_task;
    uintptr_t child_phys = process_phys_base((size_t)child);
    uintptr_t parent_phys = tasks[parent].phys_base;
    uintptr_t program_end = tasks[parent].program_end;
    uintptr_t heap_base = tasks[parent].heap_base;
    uintptr_t brk = tasks[parent].brk;
    uintptr_t heap_limit = tasks[parent].heap_limit;
    uintptr_t stack_base = tasks[parent].stack_base;
    uintptr_t stack_top = tasks[parent].stack_top;
    uintptr_t parent_pid = tasks[parent].pid;

    disable_paging();
    k_memcpy((void *)child_phys, (const void *)parent_phys, APP_LOAD_LIMIT);
    setup_address_space((size_t)child, USER_BASE, program_end, stack_base, stack_top, child_phys);
    if (brk > heap_base) {
        map_user_region(
            (size_t)child,
            heap_base,
            brk,
            child_phys + (heap_base - USER_BASE),
            PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
    }

    k_memcpy(&tasks[child].tf, tf, sizeof(*tf));
    tasks[child].tf.a0 = 0;
    tasks[child].tf.sepc += 4;
    tasks[child].state = TASK_READY;
    tasks[child].pid = (uintptr_t)child + 1;
    tasks[child].parent_pid = parent_pid;
    tasks[child].app_base = USER_BASE;
    tasks[child].phys_base = child_phys;
    tasks[child].program_end = program_end;
    tasks[child].heap_base = heap_base;
    tasks[child].brk = brk;
    tasks[child].heap_limit = heap_limit;
    tasks[child].stack_base = stack_base;
    tasks[child].stack_top = stack_top;
    tasks[child].exit_code = 0;
    k_memcpy(tasks[child].fds, tasks[parent].fds, sizeof(tasks[child].fds));
    k_memcpy(tasks[child].signal_actions, tasks[parent].signal_actions, sizeof(tasks[child].signal_actions));
    tasks[child].signal_mask = tasks[parent].signal_mask;
    tasks[child].signal_pending = 0;
    tasks[child].handling_signal = 0;
    k_memset(&tasks[child].saved_signal_tf, 0, sizeof(tasks[child].saved_signal_tf));
    clone_fd_refs((size_t)child);
    live_tasks++;
    activate_task(parent);
    return (int64_t)tasks[child].pid;
}

static int64_t sys_exec(uintptr_t path, uint64_t len)
{
    int app = find_app_by_user_path(path, len);
    if (app < 0 || current_task < 0) {
        return -1;
    }
    reset_task_image((size_t)current_task, (size_t)app);
    return 0;
}

static int64_t sys_wait(uintptr_t pid_arg, uintptr_t exit_code_ptr)
{
    if (current_task < 0) {
        return -1;
    }
    uintptr_t current_pid = tasks[current_task].pid;
    int any = pid_arg == UINTPTR_MAX;
    int matched = 0;

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].parent_pid != current_pid) {
            continue;
        }
        if (!any && tasks[i].pid != pid_arg) {
            continue;
        }
        matched = 1;
        if (tasks[i].state == TASK_EXITED || tasks[i].state == TASK_FAULTED) {
            int64_t pid = (int64_t)tasks[i].pid;
            if (exit_code_ptr != 0 && user_range_ok(exit_code_ptr, sizeof(int32_t))) {
                *(volatile int32_t *)exit_code_ptr = (int32_t)tasks[i].exit_code;
            }
            tasks[i].state = TASK_UNUSED;
            tasks[i].parent_pid = 0;
            tasks[i].exit_code = 0;
            close_all_fds(i);
            init_signal_state(i);
            return pid;
        }
    }

    return matched ? -2 : -1;
}

static void finish_kernel(struct trap_frame *tf, int64_t result)
{
    tf->ra = saved_kernel_ra;
    tf->sp = saved_kernel_sp;
    tf->a0 = (uint64_t)result;
    tf->sepc = (uintptr_t)__return_from_user;
    tf->sstatus |= SSTATUS_SPP | SSTATUS_SPIE;
}

static void save_current_task(struct trap_frame *tf)
{
    if (current_task >= 0 && (size_t)current_task < task_total) {
        k_memcpy(&tasks[current_task].tf, tf, sizeof(*tf));
    }
}

static int pick_next_task(void)
{
    if (live_tasks == 0 || task_total == 0) {
        return -1;
    }

    size_t start = current_task < 0 ? 0 : ((size_t)current_task + 1) % task_total;
    for (size_t scanned = 0; scanned < task_total; scanned++) {
        size_t index = (start + scanned) % task_total;
        if (tasks[index].state == TASK_READY) {
            return (int)index;
        }
    }
    return -1;
}

static void switch_to_task(struct trap_frame *tf, int next)
{
    current_task = next;
    tasks[next].state = TASK_RUNNING;
    k_memcpy(tf, &tasks[next].tf, sizeof(*tf));
    activate_task((size_t)next);
}

static void schedule_or_finish(struct trap_frame *tf)
{
    int next = pick_next_task();
    if (next < 0) {
        finish_kernel(tf, 0);
        return;
    }
    switch_to_task(tf, next);
}

static uint64_t initial_user_sstatus(void)
{
    uint64_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    status &= ~SSTATUS_SPP;
    status |= SSTATUS_SPIE | SSTATUS_SUM;
    return status;
}

static void init_tasks(void)
{
    if (app_count() == 0) {
        sbi_shutdown();
    }
    task_total = MAX_TASKS;
    live_tasks = 1;
    current_task = -1;
    reset_task_image(0, INITPROC_APP);
    tasks[0].state = TASK_READY;
    tasks[0].pid = 1;
    tasks[0].parent_pid = 0;
    init_standard_fds(0);
    init_signal_state(0);
}

static void run_tasks(void)
{
    init_tasks();
    int first = pick_next_task();
    if (first < 0) {
        return;
    }
    current_task = first;
    tasks[first].state = TASK_RUNNING;
    activate_task((size_t)first);
    (void)enter_task(&tasks[first].tf);
}

void trap_handler(struct trap_frame *tf)
{
    uint64_t raw_scause = tf->scause;
    uint64_t cause = raw_scause & ~SCAUSE_INTERRUPT;

    if ((raw_scause & SCAUSE_INTERRUPT) != 0) {
        save_current_task(tf);
        tasks[current_task].state = TASK_READY;
        schedule_or_finish(tf);
        return;
    }

    if (cause == SCAUSE_U_ECALL) {
        if (tf->a7 == SYS_OPENAT) {
            tf->a0 = (uint64_t)sys_open(tf->a0, tf->a1);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_CLOSE) {
            tf->a0 = (uint64_t)sys_close(tf->a0);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_PIPE2) {
            tf->a0 = (uint64_t)sys_pipe(tf->a0);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_READ) {
            tf->a0 = (uint64_t)sys_read(tf->a0, tf->a1, tf->a2);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_WRITE) {
            tf->a0 = (uint64_t)sys_write(tf->a0, tf->a1, tf->a2);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_CLOCK_GETTIME) {
            tf->a0 = (uint64_t)sys_clock_gettime(tf->a0, tf->a1);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_SCHED_YIELD) {
            tf->a0 = 0;
            tf->sepc += 4;
            deliver_pending_signal(tf);
            save_current_task(tf);
            tasks[current_task].state = TASK_READY;
            schedule_or_finish(tf);
            return;
        }

        if (tf->a7 == SYS_KILL) {
            tf->a0 = (uint64_t)sys_kill(tf->a0, tf->a1);
            tf->sepc += 4;
            deliver_pending_signal(tf);
            return;
        }

        if (tf->a7 == SYS_RT_SIGACTION) {
            tf->a0 = (uint64_t)sys_sigaction(tf->a0, tf->a1, tf->a2);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_RT_SIGPROCMASK) {
            tf->a0 = (uint64_t)sys_sigprocmask(tf->a0);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_RT_SIGRETURN) {
            sys_sigreturn(tf);
            return;
        }

        if (tf->a7 == SYS_GETPID) {
            tf->a0 = tasks[current_task].pid;
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_BRK) {
            tf->a0 = (uint64_t)sys_sbrk((int64_t)tf->a0);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_CLONE) {
            tf->a0 = (uint64_t)sys_fork(tf);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_EXECVE) {
            int64_t ret = sys_exec(tf->a0, tf->a1);
            if (ret < 0) {
                tf->a0 = (uint64_t)ret;
                tf->sepc += 4;
            } else {
                k_memcpy(tf, &tasks[current_task].tf, sizeof(*tf));
            }
            return;
        }

        if (tf->a7 == SYS_WAIT4) {
            tf->a0 = (uint64_t)sys_wait(tf->a0, tf->a1);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_EXIT) {
            tasks[current_task].exit_code = (int64_t)tf->a0;
            tasks[current_task].state = TASK_EXITED;
            if (live_tasks > 0) {
                live_tasks--;
            }
            schedule_or_finish(tf);
            return;
        }

        tf->a0 = (uint64_t)-1;
        tf->sepc += 4;
        return;
    }

    if (cause == SCAUSE_ILLEGAL_INSTRUCTION ||
        cause == SCAUSE_LOAD_ACCESS_FAULT ||
        cause == SCAUSE_STORE_ACCESS_FAULT ||
        cause == SCAUSE_LOAD_PAGE_FAULT ||
        cause == SCAUSE_STORE_PAGE_FAULT) {
        tasks[current_task].state = TASK_FAULTED;
        if (live_tasks > 0) {
            live_tasks--;
        }
        schedule_or_finish(tf);
        return;
    }

    tasks[current_task].state = TASK_FAULTED;
    if (live_tasks > 0) {
        live_tasks--;
    }
    schedule_or_finish(tf);
}

void kernel_main(uintptr_t hartid, uintptr_t opaque)
{
    (void)hartid;
    (void)opaque;

    __asm__ volatile("csrw satp, zero\nsfence.vma" ::: "memory");

    uintptr_t trap_vector = (uintptr_t)__trap_entry;
    __asm__ volatile("csrw stvec, %0" :: "r"(trap_vector) : "memory");

    boot_ticks = read_time_counter();
    logical_nsec = 0;

    run_tasks();
    sbi_shutdown();
}

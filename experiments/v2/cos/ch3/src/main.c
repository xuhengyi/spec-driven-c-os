// COS ch3 RISC-V OS snapshot generated from bundles/ch3.bundle.md.
// Freestanding supervisor-mode kernel with U-mode app loading and cooperative tasks.

#include <stddef.h>
#include <stdint.h>

#define SBI_CONSOLE_PUTCHAR 1UL
#define SBI_SHUTDOWN 8UL

#define SYS_WRITE 64UL
#define SYS_EXIT 93UL
#define SYS_CLOCK_GETTIME 113UL
#define SYS_SCHED_YIELD 124UL

#define SCAUSE_ILLEGAL_INSTRUCTION 2UL
#define SCAUSE_LOAD_ACCESS_FAULT 5UL
#define SCAUSE_STORE_ACCESS_FAULT 7UL
#define SCAUSE_U_ECALL 8UL
#define SCAUSE_LOAD_PAGE_FAULT 13UL
#define SCAUSE_STORE_PAGE_FAULT 15UL

#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SPP (1UL << 8)
#define SCAUSE_INTERRUPT (1ULL << 63)

#define MAX_TASKS 16UL
#define APP_LOAD_LIMIT 0x200000UL
#define USER_STACK_SIZE 16384UL
#define TIMEBASE_HZ 10000000ULL
#define NSEC_PER_SEC 1000000000ULL

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

struct task {
    struct trap_frame tf;
    enum task_state state;
    uintptr_t app_base;
    uintptr_t app_limit;
    uintptr_t stack_base;
    uintptr_t stack_top;
    int64_t exit_code;
};

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
static size_t task_total;
static size_t live_tasks;
static int current_task = -1;
static uint64_t boot_ticks;
static uint64_t logical_nsec;

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

static uintptr_t app_base(void)
{
    return apps[0];
}

static uintptr_t app_step(void)
{
    return apps[1];
}

static uintptr_t user_stack_top(size_t index)
{
    switch (index) {
    case 0:
        return (uintptr_t)user_stack0_top;
    case 1:
        return (uintptr_t)user_stack1_top;
    case 2:
        return (uintptr_t)user_stack2_top;
    case 3:
        return (uintptr_t)user_stack3_top;
    case 4:
        return (uintptr_t)user_stack4_top;
    case 5:
        return (uintptr_t)user_stack5_top;
    case 6:
        return (uintptr_t)user_stack6_top;
    case 7:
        return (uintptr_t)user_stack7_top;
    case 8:
        return (uintptr_t)user_stack8_top;
    case 9:
        return (uintptr_t)user_stack9_top;
    case 10:
        return (uintptr_t)user_stack10_top;
    case 11:
        return (uintptr_t)user_stack11_top;
    case 12:
        return (uintptr_t)user_stack12_top;
    case 13:
        return (uintptr_t)user_stack13_top;
    case 14:
        return (uintptr_t)user_stack14_top;
    default:
        return (uintptr_t)user_stack15_top;
    }
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
    return range_contains(task->app_base, task->app_limit, ptr, len) ||
           range_contains(task->stack_base, task->stack_top, ptr, len);
}

static int64_t sys_write(uint64_t fd, uintptr_t buf, uint64_t len)
{
    if ((fd != 1 && fd != 2) || !user_range_ok(buf, len)) {
        return -1;
    }

    const volatile char *chars = (const volatile char *)buf;
    for (uint64_t i = 0; i < len; i++) {
        sbi_putchar(chars[i]);
    }
    return (int64_t)len;
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

static uintptr_t load_app(size_t index)
{
    uintptr_t base = app_base() + app_step() * index;
    uintptr_t start = apps[3 + index];
    uintptr_t end = apps[3 + index + 1];
    uintptr_t len = end - start;

    if (len > APP_LOAD_LIMIT || app_step() < APP_LOAD_LIMIT) {
        sbi_shutdown();
    }

    volatile uint8_t *dst = (volatile uint8_t *)base;
    for (uintptr_t i = 0; i < APP_LOAD_LIMIT; i++) {
        dst[i] = 0;
    }

    const uint8_t *src = (const uint8_t *)start;
    for (uintptr_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    __asm__ volatile("fence.i" ::: "memory");
    return base;
}

static uint64_t initial_user_sstatus(void)
{
    uint64_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    status &= ~SSTATUS_SPP;
    status |= SSTATUS_SPIE;
    return status;
}

static void init_task(size_t index)
{
    uintptr_t entry = load_app(index);
    uintptr_t stack_top = user_stack_top(index);

    k_memset(&tasks[index], 0, sizeof(tasks[index]));
    tasks[index].state = TASK_READY;
    tasks[index].app_base = entry;
    tasks[index].app_limit = entry + APP_LOAD_LIMIT;
    tasks[index].stack_top = stack_top;
    tasks[index].stack_base = stack_top - USER_STACK_SIZE;
    tasks[index].tf.sp = stack_top;
    tasks[index].tf.sepc = entry;
    tasks[index].tf.sstatus = initial_user_sstatus();
    live_tasks++;
}

static void init_tasks(void)
{
    task_total = app_count();
    live_tasks = 0;
    current_task = -1;
    for (size_t i = 0; i < task_total; i++) {
        init_task(i);
    }
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
            save_current_task(tf);
            tasks[current_task].state = TASK_READY;
            schedule_or_finish(tf);
            return;
        }

        if (tf->a7 == SYS_EXIT) {
            tasks[current_task].exit_code = (int64_t)tf->a0;
            tasks[current_task].state = TASK_EXITED;
            live_tasks--;
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
        live_tasks--;
        schedule_or_finish(tf);
        return;
    }

    tasks[current_task].state = TASK_FAULTED;
    live_tasks--;
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

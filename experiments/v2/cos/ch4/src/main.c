// COS ch4 RISC-V OS snapshot generated from bundles/ch4.bundle.md.
// Freestanding supervisor-mode kernel with U-mode app loading, page tables, and sbrk.

#include <stddef.h>
#include <stdint.h>

#define SBI_CONSOLE_PUTCHAR 1UL
#define SBI_SHUTDOWN 8UL

#define SYS_WRITE 64UL
#define SYS_EXIT 93UL
#define SYS_CLOCK_GETTIME 113UL
#define SYS_SCHED_YIELD 124UL
#define SYS_BRK 214UL

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

#define MAX_TASKS 16UL
#define APP_LOAD_LIMIT 0x200000UL
#define USER_STACK_SIZE 16384UL
#define PAGE_SIZE 4096UL
#define PROGRAM_STATIC_SIZE 0x80000UL
#define KERNEL_MAP_START 0x80000000UL
#define KERNEL_MAP_END 0x86000000UL
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
    uintptr_t program_end;
    uintptr_t heap_base;
    uintptr_t brk;
    uintptr_t heap_limit;
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

static uint64_t root_tables[MAX_TASKS][512] __attribute__((aligned(4096)));
static uint64_t l1_tables[MAX_TASKS][512] __attribute__((aligned(4096)));
static uint64_t user_l0_tables[MAX_TASKS][512] __attribute__((aligned(4096)));

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

static void map_user_region(size_t index, uintptr_t start, uintptr_t end, uint64_t flags)
{
    uintptr_t va = align_down(start, PAGE_SIZE);
    uintptr_t limit = align_up(end, PAGE_SIZE);
    while (va < limit) {
        user_l0_tables[index][vpn0(va)] = pte(va, flags);
        va += PAGE_SIZE;
    }
}

static void map_user_heap(size_t index, uintptr_t start, uintptr_t end)
{
    map_user_region(
        index,
        align_down(start, PAGE_SIZE),
        align_up(end, PAGE_SIZE),
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
    uintptr_t stack_top)
{
    k_memset(root_tables[index], 0, sizeof(root_tables[index]));
    k_memset(l1_tables[index], 0, sizeof(l1_tables[index]));
    k_memset(user_l0_tables[index], 0, sizeof(user_l0_tables[index]));

    root_tables[index][vpn2(KERNEL_MAP_START)] = pte((uintptr_t)l1_tables[index], 0);
    for (uintptr_t pa = KERNEL_MAP_START; pa < KERNEL_MAP_END; pa += APP_LOAD_LIMIT) {
        l1_tables[index][vpn1(pa)] = pte(pa, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
    l1_tables[index][vpn1(base)] = pte((uintptr_t)user_l0_tables[index], 0);

    map_user_region(index, base, program_end, PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D);
    map_user_region(index, stack_base, stack_top, PTE_R | PTE_W | PTE_U | PTE_A | PTE_D);
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

    if (next > old) {
        map_user_heap((size_t)current_task, old, next);
    } else {
        unmap_user_heap((size_t)current_task, next, old);
    }
    task->brk = next;
    flush_tlb();
    return (int64_t)old;
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
    status |= SSTATUS_SPIE | SSTATUS_SUM;
    return status;
}

static void init_task(size_t index)
{
    uintptr_t entry = load_app(index);
    uintptr_t slot_limit = entry + APP_LOAD_LIMIT;
    uintptr_t stack_top = slot_limit;
    uintptr_t stack_base = stack_top - USER_STACK_SIZE;
    uintptr_t program_end = align_up(entry + PROGRAM_STATIC_SIZE, PAGE_SIZE);
    uintptr_t heap_base = program_end;
    uintptr_t heap_limit = stack_base;

    k_memset(&tasks[index], 0, sizeof(tasks[index]));
    tasks[index].state = TASK_READY;
    tasks[index].app_base = entry;
    tasks[index].program_end = program_end;
    tasks[index].heap_base = heap_base;
    tasks[index].brk = heap_base;
    tasks[index].heap_limit = heap_limit;
    tasks[index].stack_top = stack_top;
    tasks[index].stack_base = stack_base;
    tasks[index].tf.sp = stack_top;
    tasks[index].tf.sepc = entry;
    tasks[index].tf.sstatus = initial_user_sstatus();
    setup_address_space(index, entry, program_end, stack_base, stack_top);
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

        if (tf->a7 == SYS_BRK) {
            tf->a0 = (uint64_t)sys_sbrk((int64_t)tf->a0);
            tf->sepc += 4;
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

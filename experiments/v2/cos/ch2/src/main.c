// COS ch2 RISC-V OS snapshot generated from bundles/ch2.bundle.md.
// Freestanding supervisor-mode kernel.

#include <stddef.h>
#include <stdint.h>

#define SBI_CONSOLE_PUTCHAR 1UL
#define SBI_SHUTDOWN 8UL

#define SYS_WRITE 64UL
#define SYS_EXIT 93UL

#define SCAUSE_ILLEGAL_INSTRUCTION 2UL
#define SCAUSE_LOAD_ACCESS_FAULT 5UL
#define SCAUSE_STORE_ACCESS_FAULT 7UL
#define SCAUSE_U_ECALL 8UL
#define SCAUSE_LOAD_PAGE_FAULT 13UL
#define SCAUSE_STORE_PAGE_FAULT 15UL

#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SPP (1UL << 8)
#define SCAUSE_INTERRUPT (1ULL << 63)

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

extern void __trap_entry(void);
extern void __return_from_user(void);
extern int64_t enter_user(uintptr_t entry, uintptr_t stack_top);

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

#define MAX_PROGRAMS 8UL
#define APP_LOAD_LIMIT 0x200000UL

static inline uintptr_t sbi_call(uintptr_t which, uintptr_t arg0)
{
    register uintptr_t a0 __asm__("a0") = arg0;
    register uintptr_t a7 __asm__("a7") = which;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
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

static int64_t sys_write(uint64_t fd, const char *buf, uint64_t len)
{
    if ((fd != 1 && fd != 2) || buf == NULL) {
        return -1;
    }

    for (uint64_t i = 0; i < len; i++) {
        char ch = ((const volatile char *)buf)[i];
        if (ch == '\0') {
            break;
        }
        sbi_putchar(ch);
    }
    return (int64_t)len;
}

static void finish_user(struct trap_frame *tf, int64_t result)
{
    tf->ra = saved_kernel_ra;
    tf->sp = saved_kernel_sp;
    tf->a0 = (uint64_t)result;
    tf->sepc = (uintptr_t)__return_from_user;
    tf->sstatus |= SSTATUS_SPP | SSTATUS_SPIE;
}

static size_t app_count(void)
{
    return apps[2] < MAX_PROGRAMS ? (size_t)apps[2] : (size_t)MAX_PROGRAMS;
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
    default:
        return (uintptr_t)user_stack7_top;
    }
}

static uintptr_t load_app(size_t index)
{
    uintptr_t base = app_base() + app_step() * index;
    uintptr_t start = apps[3 + index];
    uintptr_t end = apps[3 + index + 1];
    uintptr_t len = end - start;

    if (len > APP_LOAD_LIMIT) {
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

void trap_handler(struct trap_frame *tf)
{
    uint64_t raw_scause = tf->scause;
    uint64_t cause = raw_scause & ~SCAUSE_INTERRUPT;

    if ((raw_scause & SCAUSE_INTERRUPT) != 0) {
        finish_user(tf, -100);
        return;
    }

    if (cause == SCAUSE_U_ECALL) {
        if (tf->a7 == SYS_WRITE) {
            tf->a0 = (uint64_t)sys_write(tf->a0, (const char *)tf->a1, tf->a2);
            tf->sepc += 4;
            return;
        }

        if (tf->a7 == SYS_EXIT) {
            finish_user(tf, (int64_t)tf->a0);
            return;
        }

        finish_user(tf, -101);
        return;
    }

    if (cause == SCAUSE_ILLEGAL_INSTRUCTION ||
        cause == SCAUSE_LOAD_ACCESS_FAULT ||
        cause == SCAUSE_STORE_ACCESS_FAULT ||
        cause == SCAUSE_LOAD_PAGE_FAULT ||
        cause == SCAUSE_STORE_PAGE_FAULT) {
        finish_user(tf, -102);
        return;
    }

    finish_user(tf, -103);
}

static void run_batch(void)
{
    for (size_t i = 0; i < app_count(); i++) {
        uintptr_t entry = load_app(i);
        (void)enter_user(entry, user_stack_top(i));
    }
}

void kernel_main(uintptr_t hartid, uintptr_t opaque)
{
    (void)hartid;
    (void)opaque;

    __asm__ volatile("csrw satp, zero\nsfence.vma" ::: "memory");

    uintptr_t trap_vector = (uintptr_t)__trap_entry;
    __asm__ volatile("csrw stvec, %0" :: "r"(trap_vector) : "memory");

    run_batch();
    sbi_shutdown();
}

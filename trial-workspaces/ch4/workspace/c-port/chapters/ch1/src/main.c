#include "rcore/console.h"
#include "rcore/equiv.h"
#include "rcore/syscalls.h"

static inline long sbi_legacy_call(long which, long arg0) {
    register long a0 asm("a0") = arg0;
    register long a7 asm("a7") = which;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

void rcore_console_putchar(int ch) {
    sbi_legacy_call(1, ch);
}

void rcore_console_puts(const char *str) {
    while (*str != '\0') {
        rcore_console_putchar(*str++);
    }
}

void kernel_main(void) {
    rcore_console_puts("rCore C port ch1 scaffold\n");
    rcore_equiv_emit("boot", "kernel", "enter", "ok", "lang=c,chapter=1");
    rcore_console_puts("RCORE_SYS_WRITE=");
    rcore_console_putchar('0' + (RCORE_SYS_WRITE / 10) % 10);
    rcore_console_putchar('0' + (RCORE_SYS_WRITE % 10));
    rcore_console_puts("\n");
    for (;;) {
        asm volatile("wfi");
    }
}

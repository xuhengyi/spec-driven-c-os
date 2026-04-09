#include "rcore/console.h"

static inline long rcore_sbi_legacy_call(long which, long arg0) {
    register long a0 asm("a0") = arg0;
    register long a7 asm("a7") = which;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

void init_console(void) {
}

void set_log_level(const char *level) {
    (void)level;
}

void _print(const char *message) {
    rcore_console_puts(message);
}

void test_log(void) {
    rcore_console_puts("console: test_log\n");
}

void rcore_console_putchar(int ch) {
    rcore_sbi_legacy_call(1, ch);
}

void rcore_console_puts(const char *str) {
    if (str == 0) {
        return;
    }
    while (*str != '\0') {
        rcore_console_putchar((unsigned char)*str++);
    }
}

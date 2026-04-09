#ifndef RCORE_CONSOLE_H
#define RCORE_CONSOLE_H

void init_console(void);
void set_log_level(const char *level);
void test_log(void);
void _print(const char *message);
void rcore_console_putchar(int ch);
void rcore_console_puts(const char *str);

#endif

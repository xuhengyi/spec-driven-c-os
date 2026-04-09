#ifndef RCORE_EQUIV_H
#define RCORE_EQUIV_H

#include "console.h"

#define RCORE_EQUIV_PREFIX "@@EQUIV@@ "

static inline void rcore_equiv_emit(const char *phase,
                                    const char *program,
                                    const char *event,
                                    const char *result,
                                    const char *payload) {
    rcore_console_puts(RCORE_EQUIV_PREFIX);
    rcore_console_puts("phase=");
    rcore_console_puts(phase);
    rcore_console_puts(" program=");
    rcore_console_puts(program);
    rcore_console_puts(" event=");
    rcore_console_puts(event);
    rcore_console_puts(" result=");
    rcore_console_puts(result);
    rcore_console_puts(" payload=");
    rcore_console_puts(payload);
    rcore_console_puts("\n");
}

#endif

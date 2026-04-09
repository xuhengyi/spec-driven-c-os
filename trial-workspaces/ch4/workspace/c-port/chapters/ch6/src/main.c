#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch6", "enter", "todo", "ch6");
    _print("PHASE_B_C_TODO: implement ch6\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

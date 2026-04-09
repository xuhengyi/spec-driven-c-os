#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch7", "enter", "todo", "ch7");
    _print("PHASE_B_C_TODO: implement ch7\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

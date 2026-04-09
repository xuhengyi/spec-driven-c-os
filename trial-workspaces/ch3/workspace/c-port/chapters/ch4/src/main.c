#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch4", "enter", "todo", "ch4");
    _print("PHASE_B_C_TODO: implement ch4\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

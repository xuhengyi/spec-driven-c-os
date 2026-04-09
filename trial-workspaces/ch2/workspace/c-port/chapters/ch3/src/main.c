#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch3", "enter", "todo", "ch3");
    _print("PHASE_B_C_TODO: implement ch3\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

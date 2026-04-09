#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch8", "enter", "todo", "ch8");
    _print("PHASE_B_C_TODO: implement ch8\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

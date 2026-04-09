#include "rcore/equiv.h"

__attribute__((noreturn)) void rust_main(void) {
    rcore_equiv_emit("boot", "ch5", "enter", "todo", "ch5");
    _print("PHASE_B_C_TODO: implement ch5\\n");
    for (;;) {
        asm volatile("wfi");
    }
}

#include "rcore/linker.h"

static const char RCORE_LINKER_SCRIPT[] =
    "OUTPUT_ARCH(riscv)\n"
    "SECTIONS {\n"
    "    .text 0x80200000 : {\n"
    "        __start = .;\n"
    "        *(.text.entry)\n"
    "        *(.text .text.*)\n"
    "    }\n"
    "    .rodata : ALIGN(4K) {\n"
    "        __rodata = .;\n"
    "        *(.rodata .rodata.*)\n"
    "        *(.srodata .srodata.*)\n"
    "    }\n"
    "    .data : ALIGN(4K) {\n"
    "        __data = .;\n"
    "        *(.data .data.*)\n"
    "        *(.sdata .sdata.*)\n"
    "    }\n"
    "    .bss : ALIGN(8) {\n"
    "        __sbss = .;\n"
    "        *(.bss .bss.*)\n"
    "        *(.sbss .sbss.*)\n"
    "        __ebss = .;\n"
    "    }\n"
    "    .boot : ALIGN(4K) {\n"
    "        __boot = .;\n"
    "        KEEP(*(.boot.stack))\n"
    "    }\n"
    "    __end = .;\n"
    "}\n";

const char *rcore_linker_script(void) {
    return RCORE_LINKER_SCRIPT;
}

struct rcore_kernel_layout rcore_kernel_layout_locate(void) {
    extern char __start[];
    extern char __rodata[];
    extern char __data[];
    extern char __sbss[];
    extern char __ebss[];
    extern char __boot[];
    extern char __end[];
    struct rcore_kernel_layout layout = {
        (uintptr_t)__start,
        (uintptr_t)__rodata,
        (uintptr_t)__data,
        (uintptr_t)__sbss,
        (uintptr_t)__ebss,
        (uintptr_t)__boot,
        (uintptr_t)__end,
    };
    return layout;
}

void rcore_zero_bss(const struct rcore_kernel_layout *layout) {
    unsigned char *ptr = (unsigned char *)layout->sbss;
    unsigned char *end = (unsigned char *)layout->ebss;
    while (ptr < end) {
        *ptr++ = 0;
    }
}

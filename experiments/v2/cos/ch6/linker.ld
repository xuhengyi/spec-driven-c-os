OUTPUT_ARCH(riscv)
ENTRY(_start)

BASE_ADDRESS = 0x80200000;

SECTIONS
{
    . = BASE_ADDRESS;
    PROVIDE(_kernel_start = .);

    .text : ALIGN(4K) {
        KEEP(*(.text.entry))
        KEEP(*(.text.trap))
        KEEP(*(.text.user))
        *(.text .text.*)
    }

    .rodata : ALIGN(4K) {
        *(.srodata .srodata.*)
        *(.rodata .rodata.*)
    }

    PROVIDE(__global_pointer$ = . + 0x800);

    .data : ALIGN(4K) {
        *(.sdata .sdata.*)
        *(.data .data.*)
    }

    .bss : ALIGN(4K) {
        PROVIDE(_bss_start = .);
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        PROVIDE(_bss_end = .);
    }

    PROVIDE(_kernel_end = .);
}

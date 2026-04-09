    .section .text.entry
    .globl _start
_start:
    la sp, __end
    call kernel_main
1:
    j 1b

    .section .boot.stack,"aw",@nobits
    .align 12
boot_stack:
    .space 16384

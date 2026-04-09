.global apps
    .section .data
    .align 3
apps:
    .quad 0x80400000
    .quad 0x0
    .quad 5
    .quad app_0_start
    .quad app_1_start
    .quad app_2_start
    .quad app_3_start
    .quad app_4_start
    .quad app_4_end

app_0_start:
    .incbin "00hello_world.bin"
app_0_end:

app_1_start:
    .incbin "01store_fault.bin"
app_1_end:

app_2_start:
    .incbin "02power.bin"
app_2_end:

app_3_start:
    .incbin "03priv_inst.bin"
app_3_end:

app_4_start:
    .incbin "04priv_csr.bin"
app_4_end:

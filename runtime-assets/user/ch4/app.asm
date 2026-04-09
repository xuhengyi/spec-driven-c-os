.global apps
    .section .data
    .align 3
apps:
    .quad 0x0
    .quad 0x0
    .quad 12
    .quad app_0_start
    .quad app_1_start
    .quad app_2_start
    .quad app_3_start
    .quad app_4_start
    .quad app_5_start
    .quad app_6_start
    .quad app_7_start
    .quad app_8_start
    .quad app_9_start
    .quad app_10_start
    .quad app_11_start
    .quad app_11_end

app_0_start:
    .incbin "00hello_world"
app_0_end:

app_1_start:
    .incbin "01store_fault"
app_1_end:

app_2_start:
    .incbin "02power"
app_2_end:

app_3_start:
    .incbin "03priv_inst"
app_3_end:

app_4_start:
    .incbin "04priv_csr"
app_4_end:

app_5_start:
    .incbin "05write_a"
app_5_end:

app_6_start:
    .incbin "06write_b"
app_6_end:

app_7_start:
    .incbin "07write_c"
app_7_end:

app_8_start:
    .incbin "08power_3"
app_8_end:

app_9_start:
    .incbin "09power_5"
app_9_end:

app_10_start:
    .incbin "10power_7"
app_10_end:

app_11_start:
    .incbin "11sleep"
app_11_end:

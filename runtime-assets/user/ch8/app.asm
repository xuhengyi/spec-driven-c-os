.global apps
    .section .data
    .align 3
apps:
    .quad 0x0
    .quad 0x0
    .quad 23
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
    .quad app_12_start
    .quad app_13_start
    .quad app_14_start
    .quad app_15_start
    .quad app_16_start
    .quad app_17_start
    .quad app_18_start
    .quad app_19_start
    .quad app_20_start
    .quad app_21_start
    .quad app_22_start
    .quad app_22_end

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
    .incbin "12forktest"
app_5_end:

app_6_start:
    .incbin "13forktree"
app_6_end:

app_7_start:
    .incbin "14forktest2"
app_7_end:

app_8_start:
    .incbin "15matrix"
app_8_end:

app_9_start:
    .incbin "user_shell"
app_9_end:

app_10_start:
    .incbin "initproc"
app_10_end:

app_11_start:
    .incbin "filetest_simple"
app_11_end:

app_12_start:
    .incbin "cat_filea"
app_12_end:

app_13_start:
    .incbin "sig_simple"
app_13_end:

app_14_start:
    .incbin "sig_simple2"
app_14_end:

app_15_start:
    .incbin "sig_ctrlc"
app_15_end:

app_16_start:
    .incbin "sig_tests"
app_16_end:

app_17_start:
    .incbin "threads"
app_17_end:

app_18_start:
    .incbin "threads_arg"
app_18_end:

app_19_start:
    .incbin "mpsc_sem"
app_19_end:

app_20_start:
    .incbin "sync_sem"
app_20_end:

app_21_start:
    .incbin "race_adder_mutex_blocking"
app_21_end:

app_22_start:
    .incbin "test_condvar"
app_22_end:

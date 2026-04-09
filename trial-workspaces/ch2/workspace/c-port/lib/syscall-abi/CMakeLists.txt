add_library(rcore_syscall_abi STATIC
    io.c
    time.c
)

rcore_target_common(rcore_syscall_abi)
target_include_directories(rcore_syscall_abi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_syscall_abi RCORE_FEATURE_KERNEL)
target_link_libraries(rcore_syscall_abi PUBLIC
    rcore_signal_defs
)

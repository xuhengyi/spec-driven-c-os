add_library(rcore_syscall_kernel_dispatch STATIC
    mod.c
)

rcore_target_common(rcore_syscall_kernel_dispatch)
target_include_directories(rcore_syscall_kernel_dispatch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_syscall_kernel_dispatch RCORE_FEATURE_KERNEL RCORE_FEATURE_FOREIGN)
target_link_libraries(rcore_syscall_kernel_dispatch PUBLIC
    rcore_syscall_abi
    rcore_kernel_context
)

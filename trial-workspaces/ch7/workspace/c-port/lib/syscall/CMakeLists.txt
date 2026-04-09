add_library(rcore_syscall STATIC
    syscall.c
)

rcore_target_common(rcore_syscall)
target_include_directories(rcore_syscall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_syscall RCORE_FEATURE_KERNEL RCORE_FEATURE_USER)
target_link_libraries(rcore_syscall PUBLIC
    rcore_syscall_abi
    rcore_syscall_user_api
    rcore_syscall_kernel_dispatch
)

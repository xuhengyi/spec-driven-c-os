add_library(rcore_syscall_user_api STATIC
    user.c
)

rcore_target_common(rcore_syscall_user_api)
target_include_directories(rcore_syscall_user_api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_syscall_user_api RCORE_FEATURE_KERNEL RCORE_FEATURE_USER)
target_link_libraries(rcore_syscall_user_api PUBLIC
    rcore_syscall_abi
)

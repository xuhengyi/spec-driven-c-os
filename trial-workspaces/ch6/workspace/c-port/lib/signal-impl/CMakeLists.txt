add_library(rcore_signal_impl STATIC
    signal_impl.c
    default_action.c
    signal_set.c
)

rcore_target_common(rcore_signal_impl)
target_include_directories(rcore_signal_impl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_signal_impl RCORE_FEATURE_KERNEL)
target_link_libraries(rcore_signal_impl PUBLIC
    rcore_signal
    rcore_signal_defs
    rcore_kernel_context_local
)

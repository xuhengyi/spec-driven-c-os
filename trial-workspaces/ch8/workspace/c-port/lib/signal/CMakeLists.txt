add_library(rcore_signal STATIC
    signal.c
    signal_result.c
)

rcore_target_common(rcore_signal)
target_include_directories(rcore_signal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_signal RCORE_FEATURE_KERNEL)
target_link_libraries(rcore_signal PUBLIC
    rcore_signal_defs
    rcore_kernel_context_local
)

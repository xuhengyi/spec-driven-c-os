add_library(rcore_signal_defs STATIC
    signal_defs.c
)

rcore_target_common(rcore_signal_defs)
target_include_directories(rcore_signal_defs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_signal_defs RCORE_FEATURE_KERNEL)

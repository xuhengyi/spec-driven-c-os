add_library(rcore_console STATIC
    console.c
)

rcore_target_common(rcore_console)
target_include_directories(rcore_console PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_console RCORE_FEATURE_KERNEL)

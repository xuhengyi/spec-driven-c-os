add_library(rcore_linker STATIC
    linker.c
    app.c
)

rcore_target_common(rcore_linker)
target_include_directories(rcore_linker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_linker RCORE_FEATURE_KERNEL)

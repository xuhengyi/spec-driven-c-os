add_library(rcore_kernel_alloc STATIC
    kernel_alloc.c
)

rcore_target_common(rcore_kernel_alloc)
target_include_directories(rcore_kernel_alloc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_kernel_alloc RCORE_FEATURE_KERNEL)

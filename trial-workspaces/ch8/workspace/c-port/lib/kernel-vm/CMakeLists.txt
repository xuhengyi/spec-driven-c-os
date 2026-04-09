add_library(rcore_kernel_vm STATIC
    kernel_vm.c
    mod.c
    mapper.c
)

rcore_target_common(rcore_kernel_vm)
target_include_directories(rcore_kernel_vm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_kernel_vm RCORE_FEATURE_KERNEL RCORE_FEATURE_FOREIGN RCORE_FEATURE_PROC)

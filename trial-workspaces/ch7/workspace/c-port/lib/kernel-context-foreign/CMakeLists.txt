add_library(rcore_kernel_context_foreign STATIC
    mod.c
    multislot_portal.c
)

rcore_target_common(rcore_kernel_context_foreign)
target_include_directories(rcore_kernel_context_foreign PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_kernel_context_foreign RCORE_FEATURE_KERNEL RCORE_FEATURE_FOREIGN)
target_link_libraries(rcore_kernel_context_foreign PUBLIC
    rcore_kernel_context_local
)

add_library(rcore_kernel_context STATIC
    kernel_context.c
    mod.c
)

rcore_target_common(rcore_kernel_context)
target_include_directories(rcore_kernel_context PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_kernel_context RCORE_FEATURE_KERNEL RCORE_FEATURE_FOREIGN)
target_link_libraries(rcore_kernel_context PUBLIC
    rcore_kernel_context_local
    rcore_kernel_context_foreign
)

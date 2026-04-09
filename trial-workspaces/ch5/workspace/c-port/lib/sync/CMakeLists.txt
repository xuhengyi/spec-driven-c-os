add_library(rcore_sync STATIC
    sync.c
)

rcore_target_common(rcore_sync)
target_include_directories(rcore_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_sync RCORE_FEATURE_KERNEL RCORE_FEATURE_THREAD)
target_link_libraries(rcore_sync PUBLIC
    rcore_sync_up_cell
    rcore_sync_blocking_primitives
)

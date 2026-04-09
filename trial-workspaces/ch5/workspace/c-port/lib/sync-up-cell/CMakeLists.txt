add_library(rcore_sync_up_cell STATIC
    up.c
)

rcore_target_common(rcore_sync_up_cell)
target_include_directories(rcore_sync_up_cell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_sync_up_cell RCORE_FEATURE_KERNEL RCORE_FEATURE_THREAD RCORE_FEATURE_COOP)

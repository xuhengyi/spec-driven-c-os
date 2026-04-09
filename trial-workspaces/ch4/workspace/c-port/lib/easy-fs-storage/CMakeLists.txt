add_library(rcore_easy_fs_storage STATIC
    block_dev.c
    block_cache.c
    layout.c
    bitmap.c
    efs.c
)

rcore_target_common(rcore_easy_fs_storage)
target_include_directories(rcore_easy_fs_storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_easy_fs_storage RCORE_FEATURE_KERNEL)

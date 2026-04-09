add_library(rcore_easy_fs_vfs STATIC
    vfs.c
    file.c
)

rcore_target_common(rcore_easy_fs_vfs)
target_include_directories(rcore_easy_fs_vfs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_easy_fs_vfs RCORE_FEATURE_KERNEL)
target_link_libraries(rcore_easy_fs_vfs PUBLIC
    rcore_easy_fs_storage
)

add_library(rcore_easy_fs STATIC
    easy_fs.c
)

rcore_target_common(rcore_easy_fs)
target_include_directories(rcore_easy_fs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_easy_fs RCORE_FEATURE_KERNEL)
target_link_libraries(rcore_easy_fs PUBLIC
    rcore_easy_fs_storage
    rcore_easy_fs_vfs
)

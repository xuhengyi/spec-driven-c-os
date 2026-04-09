add_library(rcore_task_manage STATIC
    task_manage.c
)

rcore_target_common(rcore_task_manage)
target_include_directories(rcore_task_manage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_task_manage RCORE_FEATURE_KERNEL RCORE_FEATURE_PROC RCORE_FEATURE_THREAD)
target_link_libraries(rcore_task_manage PUBLIC
    rcore_task_manage_core
    rcore_task_manage_relations
)

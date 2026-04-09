add_library(rcore_task_manage_relations STATIC
    proc_rel.c
    proc_thread_rel.c
    thread_manager.c
)

rcore_target_common(rcore_task_manage_relations)
target_include_directories(rcore_task_manage_relations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_task_manage_relations RCORE_FEATURE_KERNEL RCORE_FEATURE_PROC)
target_link_libraries(rcore_task_manage_relations PUBLIC
    rcore_task_manage_core
)

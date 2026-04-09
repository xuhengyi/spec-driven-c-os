add_library(rcore_task_manage_core STATIC
    id.c
    manager.c
    scheduler.c
)

rcore_target_common(rcore_task_manage_core)
target_include_directories(rcore_task_manage_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_task_manage_core RCORE_FEATURE_KERNEL RCORE_FEATURE_COOP)

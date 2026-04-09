add_library(rcore_sync_blocking_primitives STATIC
    mutex.c
    condvar.c
    semaphore.c
)

rcore_target_common(rcore_sync_blocking_primitives)
target_include_directories(rcore_sync_blocking_primitives PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
rcore_apply_feature_flags(rcore_sync_blocking_primitives RCORE_FEATURE_KERNEL RCORE_FEATURE_THREAD)
target_link_libraries(rcore_sync_blocking_primitives PUBLIC
    rcore_sync_up_cell
    rcore_task_manage_core
)

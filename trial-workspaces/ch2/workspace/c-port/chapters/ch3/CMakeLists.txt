add_executable(rcore_ch3_kernel
    src/main.c
    src/task.c
    src/start.S
)

rcore_target_common(rcore_ch3_kernel)
target_include_directories(rcore_ch3_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
rcore_apply_feature_flags(rcore_ch3_kernel RCORE_FEATURE_KERNEL RCORE_FEATURE_USER RCORE_FEATURE_COOP)
target_link_libraries(rcore_ch3_kernel PRIVATE
    rcore_linker
    rcore_console
    rcore_kernel_context_local
    rcore_syscall_kernel_dispatch
    rcore_task_manage_core
)
rcore_prepare_app_asm(chapter_app_asm)
target_sources(rcore_ch3_kernel PRIVATE "${chapter_app_asm}")

set_target_properties(rcore_ch3_kernel PROPERTIES
    OUTPUT_NAME "ch3"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${RCORE_ARTIFACT_DIR}"
)

target_link_options(rcore_ch3_kernel PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map=${RCORE_ARTIFACT_DIR}/ch3.map
)

add_custom_command(
    TARGET rcore_ch3_kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:rcore_ch3_kernel>
            ${RCORE_ARTIFACT_DIR}/ch3.bin
    BYPRODUCTS ${RCORE_ARTIFACT_DIR}/ch3.bin
)

add_custom_target(ch3_kernel DEPENDS rcore_ch3_kernel)

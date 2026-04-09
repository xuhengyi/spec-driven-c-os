add_executable(rcore_ch6_kernel
    src/main.c
    src/process.c
    src/processor.c
    src/fs.c
    src/virtio_block.c
    src/start.S
)

rcore_target_common(rcore_ch6_kernel)
target_include_directories(rcore_ch6_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
rcore_apply_feature_flags(rcore_ch6_kernel RCORE_FEATURE_KERNEL RCORE_FEATURE_USER RCORE_FEATURE_FOREIGN RCORE_FEATURE_PROC)
target_link_libraries(rcore_ch6_kernel PRIVATE
    rcore_linker
    rcore_console
    rcore_kernel_alloc
    rcore_kernel_vm
    rcore_kernel_context
    rcore_task_manage_relations
    rcore_easy_fs
    rcore_syscall_kernel_dispatch
)
rcore_prepare_app_asm(chapter_app_asm)
target_sources(rcore_ch6_kernel PRIVATE "${chapter_app_asm}")

set_target_properties(rcore_ch6_kernel PROPERTIES
    OUTPUT_NAME "ch6"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${RCORE_ARTIFACT_DIR}"
)

target_link_options(rcore_ch6_kernel PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map=${RCORE_ARTIFACT_DIR}/ch6.map
)

add_custom_command(
    TARGET rcore_ch6_kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:rcore_ch6_kernel>
            ${RCORE_ARTIFACT_DIR}/ch6.bin
    BYPRODUCTS ${RCORE_ARTIFACT_DIR}/ch6.bin
)

add_custom_target(ch6_kernel DEPENDS rcore_ch6_kernel)

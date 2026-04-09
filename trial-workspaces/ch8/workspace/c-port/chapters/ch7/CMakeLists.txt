add_executable(rcore_ch7_kernel
    src/main.c
    src/start.S
)

rcore_target_common(rcore_ch7_kernel)
target_include_directories(rcore_ch7_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
rcore_apply_feature_flags(rcore_ch7_kernel RCORE_FEATURE_KERNEL RCORE_FEATURE_USER)
target_link_libraries(rcore_ch7_kernel PRIVATE
    rcore_linker
    rcore_console
    rcore_kernel_context_local
)
rcore_prepare_app_asm(chapter_app_asm)
target_sources(rcore_ch7_kernel PRIVATE "${chapter_app_asm}")

set_target_properties(rcore_ch7_kernel PROPERTIES
    OUTPUT_NAME "ch7"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${RCORE_ARTIFACT_DIR}"
)

target_link_options(rcore_ch7_kernel PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map=${RCORE_ARTIFACT_DIR}/ch7.map
)

add_custom_command(
    TARGET rcore_ch7_kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:rcore_ch7_kernel>
            ${RCORE_ARTIFACT_DIR}/ch7.bin
    BYPRODUCTS ${RCORE_ARTIFACT_DIR}/ch7.bin
)

add_custom_target(ch7_kernel DEPENDS rcore_ch7_kernel)

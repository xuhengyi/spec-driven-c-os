add_executable(ch1_kernel
    src/start.S
    src/main.c
)

target_include_directories(ch1_kernel PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(ch1_kernel PRIVATE
    -Wno-unused-parameter
)

set_target_properties(ch1_kernel PROPERTIES
    OUTPUT_NAME "ch1"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${RCORE_ARTIFACT_DIR}"
)

target_link_options(ch1_kernel PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,-Map=${RCORE_ARTIFACT_DIR}/ch1.map
)

add_custom_command(
    TARGET ch1_kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:ch1_kernel>
            ${RCORE_ARTIFACT_DIR}/ch1.bin
    BYPRODUCTS ${RCORE_ARTIFACT_DIR}/ch1.bin
)

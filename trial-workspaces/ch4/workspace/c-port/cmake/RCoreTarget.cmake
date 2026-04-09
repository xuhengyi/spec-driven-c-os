function(rcore_target_common target)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE")
        target_sources(${target} PRIVATE ${PROJECT_SOURCE_DIR}/cmake/rt_memory.c)
    endif()
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/include)
    target_compile_features(${target} PRIVATE c_std_11)
    target_compile_options(${target} PRIVATE -Wno-unused-parameter)
endfunction()

function(rcore_apply_feature_flags target)
    foreach(flag IN LISTS ARGN)
        target_compile_definitions(${target} PUBLIC ${flag}=1)
    endforeach()
    foreach(global_flag IN ITEMS
        RCORE_FEATURE_USER
        RCORE_FEATURE_KERNEL
        RCORE_FEATURE_FOREIGN
        RCORE_FEATURE_PROC
        RCORE_FEATURE_THREAD
        RCORE_FEATURE_COOP)
        if(${global_flag})
            target_compile_definitions(${target} PUBLIC ${global_flag}=1)
        endif()
    endforeach()
endfunction()

function(rcore_resolve_app_asm out_var)
    set(candidates)
    if(DEFINED APP_ASM AND NOT APP_ASM STREQUAL "")
        list(APPEND candidates "${APP_ASM}")
    endif()
    if(DEFINED ENV{APP_ASM} AND NOT "$ENV{APP_ASM}" STREQUAL "")
        list(APPEND candidates "$ENV{APP_ASM}")
    endif()
    list(APPEND candidates
        "${PROJECT_SOURCE_DIR}/../target/riscv64gc-unknown-none-elf/debug/app.asm"
        "${PROJECT_SOURCE_DIR}/../target/riscv64gc-unknown-none-elf/release/app.asm")
    foreach(candidate IN LISTS candidates)
        if(NOT candidate STREQUAL "" AND EXISTS "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(
        FATAL_ERROR
        "APP_ASM is required for this chapter build. Set APP_ASM or provide ../target/riscv64gc-unknown-none-elf/{debug,release}/app.asm"
    )
endfunction()

function(rcore_prepare_app_asm out_var)
    rcore_resolve_app_asm(app_asm_input)
    get_filename_component(app_asm_source_dir "${app_asm_input}" DIRECTORY)
    set(app_asm_output "${CMAKE_CURRENT_BINARY_DIR}/generated/app.asm.S")
    get_filename_component(app_asm_generated_dir "${app_asm_output}" DIRECTORY)
    file(MAKE_DIRECTORY "${app_asm_generated_dir}")
    add_custom_command(
        OUTPUT "${app_asm_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${app_asm_source_dir}" "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${app_asm_source_dir}" "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${app_asm_input}" "${app_asm_output}"
        DEPENDS "${app_asm_input}"
        COMMENT "Staging APP_ASM payload from ${app_asm_input}"
        VERBATIM
    )
    set_source_files_properties("${app_asm_output}" PROPERTIES GENERATED TRUE LANGUAGE ASM)
    set(${out_var} "${app_asm_output}" PARENT_SCOPE)
endfunction()

include_guard(GLOBAL)

function(aero_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /FS
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /utf-8)

        if(NOT AERO_ENABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE /EHs-c-)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        endif()

        if(NOT AERO_ENABLE_RTTI)
            target_compile_options(${target} PRIVATE /GR-)
        endif()

        if(AERO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()

    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion)

        # GCC diagnoses references obtained through short-lived Span/view
        # temporaries even when the referenced storage belongs to the retained
        # owner. The same pattern exists in style and render planners and is
        # already guarded by explicit owner lifetimes.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -Wno-dangling-reference)
        endif()

        if(NOT AERO_ENABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()

        if(NOT AERO_ENABLE_RTTI)
            target_compile_options(${target} PRIVATE -fno-rtti)
        endif()

        if(AERO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()

    endif()
endfunction()

function(aero_verify_windows_exports target expected_export)
    if(NOT WIN32 OR NOT AERO_BUILD_SHARED)
        return()
    endif()
    get_filename_component(aero_msvc_tool_dir "${CMAKE_LINKER}" DIRECTORY)
    set(aero_dumpbin "${aero_msvc_tool_dir}/dumpbin.exe")
    if(NOT EXISTS "${aero_dumpbin}")
        message(FATAL_ERROR
            "Shared Windows export verification requires dumpbin.exe next to the linker")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DAERO_DUMPBIN=${aero_dumpbin}"
            "-DAERO_DLL=$<TARGET_FILE:${target}>"
            "-DAERO_EXPECTED_EXPORT=${expected_export}"
            "-DAERO_EXPORT_LOG=$<TARGET_FILE:${target}>.exports.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckWindowsExports.cmake"
        VERBATIM)
endfunction()

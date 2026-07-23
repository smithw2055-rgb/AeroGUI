function(aero_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
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

        if(NOT AERO_ENABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()

        if(NOT AERO_ENABLE_RTTI)
            target_compile_options(${target} PRIVATE -fno-rtti)
        endif()

        if(AERO_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()

        # The existing C++17 two-or-four-argument AERO_DECLARE_METADATA
        # selector intentionally invokes a variadic selector with an empty
        # trailing pack. New compiler releases diagnose that compatibility
        # form through their extension/pedantic channel. Keep all ordinary
        # warnings fatal while excluding only that legacy macro channel until
        # generated declarations replace it throughout the public headers.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -Wno-pedantic)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -Wno-error=gnu-zero-variadic-macro-arguments)
        endif()
    endif()
endfunction()

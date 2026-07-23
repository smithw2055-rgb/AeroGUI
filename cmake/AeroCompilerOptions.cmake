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

        # The existing C++17 two-or-four-argument AERO_DECLARE_METADATA
        # selector intentionally invokes a variadic selector with an empty
        # trailing pack. GCC and newer Clang releases diagnose that otherwise
        # valid compatibility macro through their pedantic extension channel.
        # Keep the warning visible but non-fatal until the generated metadata
        # declaration form replaces the selector in all public headers.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -Wno-error=pedantic)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -Wno-error=variadic-macro-arguments-omitted
                -Wno-error=gnu-zero-variadic-macro-arguments)
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

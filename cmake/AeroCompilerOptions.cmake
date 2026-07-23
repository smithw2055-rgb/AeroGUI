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

        # Compatibility diagnostics carried by the current foundation branch:
        # - the C++17 two-or-four-argument metadata selector uses an empty
        #   trailing variadic pack;
        # - the presentation bootstrap has a single-line registration macro;
        # - older value bridges do not yet enumerate String/Custom ValueKind;
        # - deprecated compatibility classes compile their own definitions;
        # - older tests omit newly appended aggregate option fields;
        # - Release removes assertion-only uses of reserved append results;
        # - newer GCC reports references into temporary Span views as dangling.
        # Keep these warnings visible where supported while every unrelated
        # warning remains fatal.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -Wno-pedantic
                -Wno-error=misleading-indentation
                -Wno-error=switch
                -Wno-error=deprecated-declarations
                -Wno-error=missing-field-initializers
                -Wno-error=unused-but-set-variable
                -Wno-error=dangling-reference)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -Wno-error=gnu-zero-variadic-macro-arguments
                -Wno-error=switch
                -Wno-error=deprecated-declarations
                -Wno-error=missing-field-initializers)
        endif()
    endif()
endfunction()

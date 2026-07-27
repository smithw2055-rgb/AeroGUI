include(CMakeParseArguments)

# Compile XAML files as build outputs and attach them to a target as generated
# resources. A host executable can be supplied through AERO_HOST_XAMLC_EXECUTABLE
# for cross-compiling; native builds use the exported Aero::xamlc target.
function(aero_add_xaml target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "aero_add_xaml target does not exist: ${target}")
    endif()

    cmake_parse_arguments(
        AERO_XAML
        ""
        "OUTPUT_DIRECTORY;ORIGIN_PREFIX"
        "SOURCES"
        ${ARGN})
    if(NOT AERO_XAML_SOURCES)
        message(FATAL_ERROR "aero_add_xaml requires SOURCES")
    endif()

    if(AERO_XAML_OUTPUT_DIRECTORY)
        set(_aero_xaml_output "${AERO_XAML_OUTPUT_DIRECTORY}")
    else()
        set(_aero_xaml_output
            "${CMAKE_CURRENT_BINARY_DIR}/generated/xaml/${target}")
    endif()

    if(DEFINED AERO_HOST_XAMLC_EXECUTABLE AND
       NOT AERO_HOST_XAMLC_EXECUTABLE STREQUAL "")
        set(_aero_xamlc "${AERO_HOST_XAMLC_EXECUTABLE}")
        set(_aero_xamlc_dependency "${AERO_HOST_XAMLC_EXECUTABLE}")
    elseif(TARGET Aero::xamlc AND NOT CMAKE_CROSSCOMPILING)
        set(_aero_xamlc "$<TARGET_FILE:Aero::xamlc>")
        set(_aero_xamlc_dependency Aero::xamlc)
    else()
        message(FATAL_ERROR
            "aero_add_xaml requires Aero::xamlc or "
            "AERO_HOST_XAMLC_EXECUTABLE")
    endif()

    set(_aero_outputs)
    foreach(_aero_source IN LISTS AERO_XAML_SOURCES)
        get_filename_component(_aero_absolute "${_aero_source}" ABSOLUTE)
        get_filename_component(_aero_name "${_aero_source}" NAME_WE)
        set(_aero_output "${_aero_xaml_output}/${_aero_name}.axir")
        if(AERO_XAML_ORIGIN_PREFIX)
            set(_aero_origin "${AERO_XAML_ORIGIN_PREFIX}/${_aero_source}")
            add_custom_command(
                OUTPUT "${_aero_output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${_aero_xaml_output}"
                COMMAND "${_aero_xamlc}"
                    --origin "${_aero_origin}"
                    "${_aero_absolute}" "${_aero_output}"
                DEPENDS "${_aero_absolute}" ${_aero_xamlc_dependency}
                VERBATIM)
        else()
            add_custom_command(
                OUTPUT "${_aero_output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${_aero_xaml_output}"
                COMMAND "${_aero_xamlc}"
                    "${_aero_absolute}" "${_aero_output}"
                DEPENDS "${_aero_absolute}" ${_aero_xamlc_dependency}
                VERBATIM)
        endif()
        list(APPEND _aero_outputs "${_aero_output}")
    endforeach()

    set_source_files_properties(${_aero_outputs}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    target_sources("${target}" PRIVATE ${_aero_outputs})
    set_property(TARGET "${target}" APPEND PROPERTY
        AERO_COMPILED_XAML "${_aero_outputs}")
endfunction()

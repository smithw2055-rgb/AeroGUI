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
        "OUTPUT_DIRECTORY;ORIGIN_PREFIX;SCHEMA;OUTPUTS_VAR"
        "SOURCES"
        ${ARGN})
    if(NOT AERO_XAML_SOURCES)
        message(FATAL_ERROR "aero_add_xaml requires SOURCES")
    endif()

    if(AERO_XAML_OUTPUT_DIRECTORY)
        get_filename_component(
            _aero_xaml_output "${AERO_XAML_OUTPUT_DIRECTORY}" ABSOLUTE)
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

    set(_aero_schema_arguments)
    set(_aero_schema_dependency)
    if(AERO_XAML_SCHEMA)
        get_filename_component(
            _aero_schema_absolute "${AERO_XAML_SCHEMA}" ABSOLUTE)
        list(APPEND _aero_schema_arguments
            --schema "${_aero_schema_absolute}")
        set(_aero_schema_dependency "${_aero_schema_absolute}")
    endif()

    set(_aero_outputs)
    foreach(_aero_source IN LISTS AERO_XAML_SOURCES)
        get_filename_component(_aero_absolute "${_aero_source}" ABSOLUTE)
        if(IS_ABSOLUTE "${_aero_source}")
            file(RELATIVE_PATH _aero_relative
                "${CMAKE_CURRENT_SOURCE_DIR}" "${_aero_absolute}")
        else()
            set(_aero_relative "${_aero_source}")
        endif()
        string(REPLACE "\\" "/" _aero_relative "${_aero_relative}")
        if(_aero_relative MATCHES "^\\.\\./" OR
           _aero_relative MATCHES "^[A-Za-z]:/")
            string(SHA256 _aero_external_hash "${_aero_absolute}")
            string(SUBSTRING "${_aero_external_hash}" 0 16
                _aero_external_hash)
            get_filename_component(_aero_source_name
                "${_aero_absolute}" NAME)
            set(_aero_relative
                "external/${_aero_external_hash}/${_aero_source_name}")
        endif()
        get_filename_component(_aero_relative_dir
            "${_aero_relative}" DIRECTORY)
        get_filename_component(_aero_name "${_aero_relative}" NAME_WE)
        if(_aero_relative_dir STREQUAL "")
            set(_aero_output_dir "${_aero_xaml_output}")
        else()
            set(_aero_output_dir
                "${_aero_xaml_output}/${_aero_relative_dir}")
        endif()
        set(_aero_output "${_aero_output_dir}/${_aero_name}.axir")
        set(_aero_depfile "${_aero_output}.d")
        set(_aero_origin_arguments)
        if(AERO_XAML_ORIGIN_PREFIX)
            list(APPEND _aero_origin_arguments
                --origin "${AERO_XAML_ORIGIN_PREFIX}/${_aero_relative}")
        endif()
        set(_aero_depfile_arguments)
        set(_aero_depfile_property)
        if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
            list(APPEND _aero_depfile_arguments
                --depfile "${_aero_depfile}")
            set(_aero_depfile_property DEPFILE "${_aero_depfile}")
        endif()
        add_custom_command(
            OUTPUT "${_aero_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${_aero_output_dir}"
            COMMAND "${_aero_xamlc}"
                ${_aero_schema_arguments}
                ${_aero_origin_arguments}
                ${_aero_depfile_arguments}
                "${_aero_absolute}" "${_aero_output}"
            DEPENDS
                "${_aero_absolute}"
                ${_aero_xamlc_dependency}
                ${_aero_schema_dependency}
            ${_aero_depfile_property}
            VERBATIM)
        list(APPEND _aero_outputs "${_aero_output}")
    endforeach()

    set_source_files_properties(${_aero_outputs}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    target_sources("${target}" PRIVATE ${_aero_outputs})
    set_property(TARGET "${target}" APPEND PROPERTY
        AERO_COMPILED_XAML "${_aero_outputs}")
    set("${target}_XAML_OUTPUTS" "${_aero_outputs}" PARENT_SCOPE)
    if(AERO_XAML_OUTPUTS_VAR)
        set("${AERO_XAML_OUTPUTS_VAR}" "${_aero_outputs}" PARENT_SCOPE)
    endif()
endfunction()

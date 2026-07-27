include(CMakeParseArguments)

# Build a host-side schema generator for an application module set. The module
# function must have this signature:
#
#   Aero::Base::Result<void> RegisterModules(
#       Aero::ModuleCatalog& modules) noexcept;
#
# The generated .aeroschema contains descriptor data only; target factories and
# callbacks remain in the runtime libraries and never enter the host-tool file.
function(aero_add_schema_manifest target)
    cmake_parse_arguments(
        AERO_SCHEMA
        ""
        "OUTPUT;MODULE_HEADER;MODULE_FUNCTION"
        "LIBRARIES"
        ${ARGN})

    if(TARGET "${target}")
        message(FATAL_ERROR
            "aero_add_schema_manifest target already exists: ${target}")
    endif()
    if(NOT AERO_SCHEMA_OUTPUT)
        message(FATAL_ERROR
            "aero_add_schema_manifest requires OUTPUT")
    endif()
    if(NOT AERO_SCHEMA_MODULE_HEADER OR
       NOT AERO_SCHEMA_MODULE_FUNCTION)
        message(FATAL_ERROR
            "aero_add_schema_manifest requires MODULE_HEADER and "
            "MODULE_FUNCTION")
    endif()
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "aero_add_schema_manifest builds and executes a host tool. "
            "Generate the manifest in a native host-tools build and pass "
            "the resulting file to aero_add_xaml(SCHEMA ...).")
    endif()

    get_filename_component(
        _aero_schema_output "${AERO_SCHEMA_OUTPUT}" ABSOLUTE)
    get_filename_component(
        _aero_schema_output_dir "${_aero_schema_output}" DIRECTORY)
    set(_aero_schema_generator
        "${target}_generator")
    set(_aero_schema_source
        "${CMAKE_CURRENT_BINARY_DIR}/generated/aero-schema/${target}.cpp")

    set(AERO_SCHEMA_GENERATED_HEADER
        "${AERO_SCHEMA_MODULE_HEADER}")
    set(AERO_SCHEMA_GENERATED_FUNCTION
        "${AERO_SCHEMA_MODULE_FUNCTION}")
    set(_aero_schema_template [=[
#include <Aero/Markup/Schema.hpp>
#include <Aero/Module.hpp>
#include <Aero/SchemaBundle.hpp>
#include <@AERO_SCHEMA_GENERATED_HEADER@>

#include <cstdio>
#include <cstdint>
#include <fstream>

namespace {
int Fail(Aero::Base::Status status) noexcept {
    const char* message = status.message != nullptr && status.message[0] != '\0'
        ? status.message : "operation failed";
    std::fprintf(stderr, "schema generator: %s\n", message);
    return 1;
}

bool WriteFile(
    const char* path,
    Aero::Base::Span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(
        reinterpret_cast<const char*>(bytes.Data()),
        static_cast<std::streamsize>(bytes.Size()));
    return static_cast<bool>(output);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "schema generator: output path is required\n");
        return 2;
    }
    Aero::ModuleCatalog modules;
    Aero::Base::Result<void> status =
        @AERO_SCHEMA_GENERATED_FUNCTION@(modules);
    if (!status) return Fail(status.GetStatus());

    Aero::SchemaBundle bundle;
    status = bundle.Prepare(modules);
    if (!status) return Fail(status.GetStatus());
    status = bundle.Finalize(modules, {});
    if (!status) return Fail(status.GetStatus());

    Aero::Base::Result<Aero::Markup::XamlSchemaManifest> manifest =
        Aero::Markup::XamlSchemaManifest::Capture(bundle.XamlSchema());
    if (!manifest) return Fail(manifest.GetStatus());
    Aero::Base::Result<Aero::Base::Vector<std::uint8_t>> encoded =
        manifest.Value().Serialize();
    if (!encoded) return Fail(encoded.GetStatus());
    if (!WriteFile(argv[1], {
            encoded.Value().Data(), encoded.Value().Size()})) {
        std::fprintf(stderr, "schema generator: cannot write output\n");
        return 1;
    }
    return 0;
}
]=])
    string(CONFIGURE "${_aero_schema_template}"
        _aero_schema_content @ONLY)
    file(GENERATE
        OUTPUT "${_aero_schema_source}"
        CONTENT "${_aero_schema_content}")

    add_executable("${_aero_schema_generator}" EXCLUDE_FROM_ALL
        "${_aero_schema_source}")
    target_link_libraries("${_aero_schema_generator}"
        PRIVATE Aero::ModuleCatalog ${AERO_SCHEMA_LIBRARIES})
    target_compile_features("${_aero_schema_generator}"
        PRIVATE cxx_std_17)
    aero_apply_compiler_options("${_aero_schema_generator}")
    set_target_properties("${_aero_schema_generator}" PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)

    add_custom_command(
        OUTPUT "${_aero_schema_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_schema_output_dir}"
        COMMAND "$<TARGET_FILE:${_aero_schema_generator}>"
            "${_aero_schema_output}"
        DEPENDS "${_aero_schema_generator}"
        VERBATIM)
    add_custom_target("${target}"
        DEPENDS "${_aero_schema_output}")
    set_property(TARGET "${target}" PROPERTY
        AERO_SCHEMA_MANIFEST "${_aero_schema_output}")
    set("${target}_OUTPUT" "${_aero_schema_output}" PARENT_SCOPE)
endfunction()

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

if(NOT DEFINED AERO_SOURCE_DIR)
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

function(aero_collect_matches output pattern)
    set(matches)
    foreach(path IN LISTS ARGN)
        file(READ "${path}" content)
        if(content MATCHES "${pattern}")
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND matches "${relative}")
        endif()
    endforeach()
    set(${output} "${matches}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE core_files
    "${AERO_SOURCE_DIR}/src/core/*.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Events/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Metadata/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Property/*.hpp")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/include/Aero/Core/Diagnostics.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Dispatcher.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/ObjectServices.hpp")
aero_collect_matches(core_reverse
    "#[ \t]*include[ \t]*<Aero/(Presentation|Controls)/"
    ${core_files})
if(core_reverse)
    message(FATAL_ERROR
        "Core must not include Presentation or Controls: ${core_reverse}")
endif()

file(GLOB_RECURSE presentation_files
    "${AERO_SOURCE_DIR}/src/presentation/*.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Presentation/*.hpp")
aero_collect_matches(presentation_reverse
    "#[ \t]*include[ \t]*<Aero/Controls/"
    ${presentation_files})
if(presentation_reverse)
    message(FATAL_ERROR
        "Presentation must not include Controls: ${presentation_reverse}")
endif()

file(GLOB_RECURSE text_files
    "${AERO_SOURCE_DIR}/src/text/*.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Text/*.hpp")
aero_collect_matches(text_reverse
    "#[ \t]*include[ \t]*<Aero/(Core|Presentation|Controls|Markup|Render|Rhi)/"
    ${text_files})
if(text_reverse)
    message(FATAL_ERROR
        "Text provider contracts must depend only on Base: ${text_reverse}")
endif()

file(GLOB_RECURSE rhi_public_files
    "${AERO_SOURCE_DIR}/include/Aero/Rhi/*.hpp")
aero_collect_matches(rhi_reverse
    "#[ \t]*include[ \t]*<Aero/(Core|Presentation|Controls|Markup|Render|Text)/"
    ${rhi_public_files})
if(rhi_reverse)
    message(FATAL_ERROR
        "RHI public contracts must depend only on Base and RHI: ${rhi_reverse}")
endif()


foreach(removed_path IN ITEMS
    "include/Aero/Markup/Runtime"
    "include/Aero/Markup/Schema"
    "include/Aero/Markup/Parsing"
    "include/Aero/Markup/Resources"
    "include/Aero/Markup/Extensions"
    "include/Aero/Markup/Compiled"
    "src/markup/runtime"
    "src/markup/schema"
    "src/markup/parsing"
    "src/markup/resources"
    "src/markup/extensions"
    "src/markup/compiled"
    "include/Aero/Markup/DocumentCache.hpp"
    "include/Aero/Core/Metadata/Activation.hpp"
    "include/Aero/Core/Metadata/MetadataDescriptors.hpp"
    "include/Aero/Core/Metadata/MetadataDsl.hpp"
    "include/Aero/Core/Metadata/MetaRegistrationContext.hpp"
    "include/Aero/Core/Metadata/MetadataValueFacets.hpp"
    "src/core/metadata/LegacyActivation.hpp"
    "src/core/metadata/MetadataDescriptors.cpp"
    "src/markup/LoaderEngine.hpp"
    "src/markup/LoaderEngine.cpp"
    "include/Aero/Markup/RuntimeHost.hpp"
    "include/Aero/Markup/XamlThemeResources.hpp"
    "include/Aero/Markup/XamlModuleSdk.hpp"
    "include/Aero/RuntimeServices.hpp"
    "src/markup/RuntimeHost.inc"
    "src/markup/RuntimeWindow.inc"
    "src/markup/RuntimeSafety.inc"
    "src/markup/RuntimeServices.inc"
    "src/markup/XamlThemeResources.hpp"
    "src/markup/XamlThemeResources.cpp"
    "include/Aero/Markup/Extensions/XamlDependencyObjectResolver.hpp")
    if(EXISTS "${AERO_SOURCE_DIR}/${removed_path}")
        message(FATAL_ERROR
            "Removed runtime/markup compatibility file still exists: ${removed_path}")
    endif()
endforeach()

set(legacy_markup_header_pattern
    "#[ \t]*include[ \t]*<Aero/Markup/(Runtime|Schema|Parsing|Resources|Extensions|Compiled)/")
file(GLOB_RECURSE production_code
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inc"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Base/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Markup/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Presentation/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Render/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Rhi/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Text/*.hpp")
aero_collect_matches(legacy_markup_includes
    "${legacy_markup_header_pattern}" ${production_code})
if(legacy_markup_includes)
    message(FATAL_ERROR
        "Production code must not include removed Markup headers: "
        "${legacy_markup_includes}")
endif()

set(legacy_header_pattern
    "#[ \t]*include[ \t]*<Aero/Core/(Activation|BuiltinTypeIds|DependencyProperty|EffectiveValueEngine|MetadataBehaviorRegistrationStore|MetadataDescriptors|MetadataDomain|MetadataDsl|MetadataId|MetadataRegistrationValues|MetadataRuntime|MetadataValueFacets|MetadataValuePath|MetadataValueRegistrationStore|TypeRegistry|Value|Binding|Input|Layout|ObjectTree|Rendering|Style|Presentation|RuntimeMetadata|ControlPrimitives|Controls)\\.hpp>")
file(GLOB_RECURSE current_code
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inc"
    "${AERO_SOURCE_DIR}/tests/*.cpp"
    "${AERO_SOURCE_DIR}/tests/*.inc"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Base/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Markup/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Presentation/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Render/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Rhi/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Text/*.hpp")
aero_collect_matches(legacy_includes "${legacy_header_pattern}" ${current_code})
if(legacy_includes)
    message(FATAL_ERROR
        "Code must not include removed legacy Core headers: ${legacy_includes}")
endif()


set(markup_kernel_files
    "${AERO_SOURCE_DIR}/src/markup/CompiledCache.cpp"
    "${AERO_SOURCE_DIR}/src/markup/CompiledDocument.cpp"
    "${AERO_SOURCE_DIR}/src/markup/ExpatXmlTokenizer.cpp"
    "${AERO_SOURCE_DIR}/src/markup/NodeReader.cpp"
    "${AERO_SOURCE_DIR}/src/markup/XmlTokenizer.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Markup/CompiledDocument.hpp")
aero_collect_matches(markup_kernel_reverse
    "#[ \t]*include[ \t]*<Aero/(Presentation|Controls|Markup/(Loader|Resources|Schema|Extensions))[.]hpp>"
    ${markup_kernel_files})
if(markup_kernel_reverse)
    message(FATAL_ERROR
        "Markup kernel must not depend on UI/runtime integration: ${markup_kernel_reverse}")
endif()

file(GLOB_RECURSE markup_translation_units
    "${AERO_SOURCE_DIR}/src/markup/*.cpp")
aero_collect_matches(markup_source_includes
    "#[ \\t]*include[ \\t]*[\"<][^\">]*\\.cpp[\">]"
    ${markup_translation_units})
if(markup_source_includes)
    message(FATAL_ERROR
        "Markup translation units must not include other .cpp files: ${markup_source_includes}")
endif()

aero_collect_matches(theme_private_pipeline
    "Theme(XamlDocument|ResourceDictionary|VisualNode)"
    ${current_code})
if(theme_private_pipeline)
    message(FATAL_ERROR
        "Built-in themes must use metadata objects and ObjectWriter: ${theme_private_pipeline}")
endif()


set(removed_runtime_include_pattern
    "#[ \t]*include[ \t]*<Aero/(Markup/(RuntimeHost|XamlModuleSdk)|RuntimeServices)[.]hpp>")
aero_collect_matches(removed_runtime_includes
    "${removed_runtime_include_pattern}" ${current_code})
if(removed_runtime_includes)
    message(FATAL_ERROR
        "Code must not include removed runtime/markup compatibility headers: ${removed_runtime_includes}")
endif()

message(STATUS "Aero architecture dependency checks passed")

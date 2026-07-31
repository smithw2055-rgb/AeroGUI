if(NOT DEFINED AERO_SOURCE_DIR)
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")

# The physical public tree and the installed SDK whitelist are one boundary.
# Internal headers must move under src/, not merely disappear from packaging.
file(GLOB_RECURSE aero_actual_public_headers
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h"
    "${AERO_SOURCE_DIR}/include/Aero/*.inl")
set(aero_declared_public_headers ${AERO_PUBLIC_HEADERS})
list(SORT aero_actual_public_headers)
list(SORT aero_declared_public_headers)
if(NOT "${aero_actual_public_headers}" STREQUAL
       "${aero_declared_public_headers}")
    message(FATAL_ERROR
        "Public header tree and install whitelist differ. "
        "Actual: ${aero_actual_public_headers}; "
        "Declared: ${aero_declared_public_headers}")
endif()

file(GLOB_RECURSE aero_public_detail_headers
    "${AERO_SOURCE_DIR}/include/Aero/Detail/*.hpp")
if(aero_public_detail_headers)
    message(FATAL_ERROR
        "Private implementation headers must not live under include/Aero/Detail: "
        "${aero_public_detail_headers}")
endif()

file(GLOB aero_root_public_headers
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
list(LENGTH aero_root_public_headers aero_root_public_header_count)
if(aero_root_public_header_count GREATER 32)
    message(FATAL_ERROR
        "Top-level Aero public header budget exceeded: "
        "${aero_root_public_header_count} > 32")
endif()

file(GLOB aero_control_public_headers
    "${AERO_SOURCE_DIR}/include/Aero/Controls/*.hpp")
list(LENGTH aero_control_public_headers aero_control_public_header_count)
if(NOT aero_control_public_header_count EQUAL 6)
    message(FATAL_ERROR
        "Controls must retain the six canonical family headers; found "
        "${aero_control_public_header_count}")
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

function(aero_collect_duplicate_includes output)
    set(matches)
    foreach(relative IN LISTS ARGN)
        set(path "${AERO_SOURCE_DIR}/${relative}")
        file(STRINGS "${path}" includes
            REGEX "^[ \t]*#[ \t]*include[ \t]+")
        set(seen)
        foreach(include_line IN LISTS includes)
            string(STRIP "${include_line}" include_line)
            list(FIND seen "${include_line}" existing)
            if(NOT existing EQUAL -1)
                list(APPEND matches "${relative}: ${include_line}")
            else()
                list(APPEND seen "${include_line}")
            endif()
        endforeach()
    endforeach()
    set(${output} "${matches}" PARENT_SCOPE)
endfunction()

aero_collect_duplicate_includes(aero_duplicate_public_includes
    ${AERO_PUBLIC_HEADERS})
if(aero_duplicate_public_includes)
    message(FATAL_ERROR
        "Public headers contain duplicate direct includes: "
        "${aero_duplicate_public_includes}")
endif()

file(GLOB_RECURSE core_files
    "${AERO_SOURCE_DIR}/src/core/*.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Metadata/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Property/*.hpp")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/include/Aero/Core/RoutedEvent.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Diagnostics.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Dispatcher.hpp"
    "${AERO_SOURCE_DIR}/src/core/ObjectServices.hpp")
aero_collect_matches(core_reverse
    "#[ \t]*include[ \t]*<Aero/Controls/"
    ${core_files})
if(core_reverse)
    message(FATAL_ERROR
        "Core must not include Controls: ${core_reverse}")
endif()

file(GLOB_RECURSE text_files
    "${AERO_SOURCE_DIR}/src/text/*.cpp"
    "${AERO_SOURCE_DIR}/include/Aero/Text/*.hpp")
aero_collect_matches(text_reverse
    "#[ \t]*include[ \t]*<Aero/(Core|Controls|Markup|Render|Rhi)/"
    ${text_files})
if(text_reverse)
    message(FATAL_ERROR
        "Text provider contracts must depend only on Base: ${text_reverse}")
endif()

file(GLOB_RECURSE rhi_public_files
    "${AERO_SOURCE_DIR}/include/Aero/Rhi/*.hpp")
aero_collect_matches(rhi_reverse
    "#[ \t]*include[ \t]*<Aero/(Core|Controls|Markup|Render|Text)/"
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

foreach(removed_sdk_path IN ITEMS
    "include/Aero/App/ApplicationHost.hpp"
    "include/Aero/App/Services.hpp"
    "include/Aero/App/Fwd.hpp"
    "include/Aero/App/Application.hpp"
    "include/Aero/App/Window.hpp"
    "include/Aero/ModuleSdk.hpp"
    "include/Aero/Metadata.hpp"
    "include/Aero/Core/Property/PropertyProviderSession.hpp"
    "include/Aero/Detail/UiMetadata.hpp"
    "include/Aero/RuntimeHost.hpp"
    "include/Aero/XamlReloadCoordinator.hpp"
    "include/Aero/BuiltinModules.hpp"
    "include/Aero/SchemaBundle.hpp"
    "include/Aero/Markup/Loader.hpp"
    "include/Aero/Markup/Extensions.hpp"
    "include/Aero/Core/Events/RoutedEventCatalog.hpp"
    "include/Aero/Core/Metadata/Detail/DescriptionBuilder.hpp"
    "include/Aero/Controls/TextBlockLayoutService.hpp"
    "include/Aero/Render/Renderer.hpp"
    "include/Aero/Render/D3D11RendererBackend.hpp"
    "include/Aero/Render/OpenGL33RendererBackend.hpp"
    "include/Aero/Render/ProductionRendering.hpp"
    "include/Aero/Render/TextBlockRenderService.hpp"
    "include/Aero/Render/D3D11TextBlockRenderService.hpp"
    "include/Aero/Rhi/BackendLifecycle.hpp"
    "include/Aero/Rhi/D3D11Backend.hpp"
    "include/Aero/Rhi/GlxSurface.hpp"
    "include/Aero/Rhi/Graphics.hpp"
    "include/Aero/Rhi/OpenGL33.hpp"
    "include/Aero/Rhi/OpenGL33Backend.hpp"
    "include/Aero/Rhi/OpenGL33State.hpp"
    "include/Aero/Rhi/Rhi.hpp"
    "include/Aero/Rhi/Surface.hpp"
    "include/Aero/Rhi/WglSurface.hpp"
    "include/Aero/Drawing.hpp"
    "include/Aero/ObjectTree.hpp"
    "include/Aero/Rendering.hpp"
    "include/Aero/Data/Binding.hpp"
    "include/Aero/Documents/Documents.hpp"
    "include/Aero/Input/Commands.hpp"
    "include/Aero/Input/Navigation.hpp"
    "include/Aero/Media/Animation.hpp"
    "include/Aero/Core/Metadata/BindingPath.hpp"
    "include/Aero/Core/Metadata/BuiltinTypeIds.hpp"
    "include/Aero/Core/Metadata/CoreMetadata.hpp"
    "include/Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp"
    "include/Aero/Core/Metadata/MetadataValuePath.hpp"
    "include/Aero/Core/Metadata/MetadataValueRegistrationStore.hpp"
    "include/Aero/Core/ObjectServices.hpp"
    "include/Aero/Core/Property/EffectiveValueEngine.hpp"
    "include/Aero/Platform/Win32Window.hpp"
    "include/Aero/Platform/X11Window.hpp"
    "include/Aero/RuntimeSafety.hpp"
    "include/Aero/Controls/Bars.hpp"
    "include/Aero/Controls/Buttons.hpp"
    "include/Aero/Controls/ContentControls.hpp"
    "include/Aero/Controls/ControlPrimitives.hpp"
    "include/Aero/Controls/Controls.hpp"
    "include/Aero/Controls/Images.hpp"
    "include/Aero/Controls/ListView.hpp"
    "include/Aero/Controls/Menus.hpp"
    "include/Aero/Controls/Metadata.hpp"
    "include/Aero/Controls/Scroll.hpp"
    "include/Aero/Controls/Selection.hpp"
    "include/Aero/Controls/Shapes.hpp"
    "include/Aero/Controls/Templates.hpp"
    "include/Aero/Controls/TextBox.hpp"
    "include/Aero/Controls/Trees.hpp"
    "include/Aero/Controls/Virtualization.hpp"
    "include/Aero/Controls/VisualStates.hpp")
    if(EXISTS "${AERO_SOURCE_DIR}/${removed_sdk_path}")
        message(FATAL_ERROR
            "Removed SDK entry still exists: ${removed_sdk_path}")
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
    "#[ \t]*include[ \t]*<Aero/Core/(Activation|BuiltinTypeIds|DependencyProperty|EffectiveValueEngine|MetadataBehaviorRegistrationStore|MetadataDescriptors|MetadataDomain|MetadataDsl|MetadataId|MetadataRegistrationValues|MetadataRuntime|MetadataValueFacets|MetadataValuePath|MetadataValueRegistrationStore|TypeRegistry|Value|Binding|Input|Layout|ObjectTree|Rendering|Style|UI|RuntimeMetadata|ControlPrimitives|Controls)\\.hpp>")
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
    "#[ \t]*include[ \t]*<Aero/(Controls|Markup/(Loader|Resources|Schema|Extensions))[.]hpp>"
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

set(sdk_entry_headers
    "${AERO_SOURCE_DIR}/include/Aero/Gui.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/App.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Meta.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Module.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/ViewHost.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/RenderEndpoint.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/SourceProvider.hpp")
aero_collect_matches(sdk_entry_leaks
    "(RuntimeHost|RenderPlan|IRenderBackend|RhiDevice|SurfaceSession|Presenter|[A-Za-z]+Manager|[A-Za-z]+Registry|[A-Za-z]+Store|[A-Za-z]+Program|DocumentCache|TransactionCallback)"
    ${sdk_entry_headers})
if(sdk_entry_leaks)
    message(FATAL_ERROR
        "Default SDK headers expose runtime implementation types: ${sdk_entry_leaks}")
endif()

set(wpf_authoring_headers
    "${AERO_SOURCE_DIR}/include/Aero/Application.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Window.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Data.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Input.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/DrawingContext.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Style.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Styling.hpp")
aero_collect_matches(wpf_authoring_leaks
    "(ApplicationHost|IApplicationPeer|IWindowPeer|CommandManager|BindingDescriptor|MetadataBindingDescriptor|PropertyProviderSession|BuildDisplayList|DependencyPropertyRegistry[ \t]*[&*])"
    ${wpf_authoring_headers})
if(wpf_authoring_leaks)
    message(FATAL_ERROR
        "WPF authoring headers expose runtime implementation types: "
        "${wpf_authoring_leaks}")
endif()

file(GLOB_RECURSE default_sdk_headers
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Core/Metadata/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/*.hpp")

set(multiline_static_members)
foreach(path IN LISTS default_sdk_headers)
    file(STRINGS "${path}" public_header_lines)
    set(public_header_line_number 0)
    foreach(line IN LISTS public_header_lines)
        math(EXPR public_header_line_number "${public_header_line_number} + 1")
        if(line MATCHES "inline[ \t]+static[ \t]+constexpr.*(Property|Event)" AND
           NOT line MATCHES ";[ \t]*$")
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND multiline_static_members
                "${relative}:${public_header_line_number}")
        endif()
    endforeach()
endforeach()
if(multiline_static_members)
    message(FATAL_ERROR
        "DependencyProperty and routed-event static definitions must stay on one line: "
        "${multiline_static_members}")
endif()
aero_collect_matches(removed_public_services
    "(RoutedEventCatalog|DescriptionBuilder|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RenderCommand|RenderImageId|RenderMeshId|RenderGlyphRunId|ThemeStyleRegistry|PPAAOutProperty|PasswordLengthProperty|RuntimeManagersFwd|Aero/Detail/|BuildEditorDisplayList|RuntimeAnimation\\(|RuntimeFrame\\(|RuntimeEasing\\(|ItemContainerGeneratorImpl[ \t]*[*]|VisualStateManagerImpl[ \t]*[*])"
    ${default_sdk_headers})
if(removed_public_services)
    message(FATAL_ERROR
        "Removed manager, metadata detail, or text service leaks through public headers: "
        "${removed_public_services}")
endif()

set(control_runtime_attachment_headers
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Base.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Items.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Panels.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Primitives.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Standard.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Text.hpp")
aero_collect_matches(typed_control_runtime_attachments
    "(ControlInteractionManager|MenuInteractionManager|ScrollInteractionManager|ListBoxInteractionManager|ComboBoxInteractionManager|TreeViewInteractionManager)[ \t]*[*]"
    ${control_runtime_attachment_headers})
if(typed_control_runtime_attachments)
    message(FATAL_ERROR
        "Public controls expose typed runtime-manager attachments: "
        "${typed_control_runtime_attachments}")
endif()

file(GLOB_RECURSE sdk_naming_files
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*/*.cpp"
    "${AERO_SOURCE_DIR}/src/*/*.hpp"
    "${AERO_SOURCE_DIR}/samples/*.cpp"
    "${AERO_SOURCE_DIR}/samples/*.hpp"
    "${AERO_SOURCE_DIR}/samples/*/*.cpp"
    "${AERO_SOURCE_DIR}/samples/*/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*/*.hpp"
    "${AERO_SOURCE_DIR}/docs/*.md")
list(APPEND sdk_naming_files
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/README.md")
aero_collect_matches(removed_sdk_names
    "(Aero::EngineHost|AeroEngineHost|RuntimeHost|RuntimeView|Advanced Host SDK|Aero::ModuleSdk([^A-Za-z0-9_]|$)|Aero::IntegrationSdk([^A-Za-z0-9_]|$)|AeroModuleSdk|AeroIntegrationSdk|App::Services([^A-Za-z0-9_]|$)|ApplicationHost([^A-Za-z0-9_]|$))"
    ${sdk_naming_files})
if(removed_sdk_names)
    message(FATAL_ERROR
        "Removed SDK naming remains in product code or documentation: ${removed_sdk_names}")
endif()

aero_collect_matches(removed_cmake_aliases
    "add_library\\([ \t\r\n]*Aero::(Rhi|Render|ModuleCatalog)([A-Za-z0-9_]|[ \t\r\n])"
    "${AERO_SOURCE_DIR}/CMakeLists.txt")
if(removed_cmake_aliases)
    message(FATAL_ERROR
        "Removed public RHI, render, or module-catalog CMake aliases remain: "
        "${removed_cmake_aliases}")
endif()

file(GLOB_RECURSE retired_surface_files
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_matches(retired_ui
    "Aero::UI([^A-Za-z0-9_]|$)|Aero/UI/|AeroUI"
    ${retired_surface_files}
    "${AERO_SOURCE_DIR}/CMakeLists.txt")
if(retired_ui)
    message(FATAL_ERROR
        "Retired UI layer remains: ${retired_ui}")
endif()

message(STATUS "Aero architecture dependency checks passed")

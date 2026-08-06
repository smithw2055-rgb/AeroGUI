if(NOT DEFINED AERO_SOURCE_DIR)
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")
include("${AERO_SOURCE_DIR}/cmake/AeroPublicNamespaces.cmake")

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

function(aero_collect_public_namespace_declarations output)
    set(matches)
    foreach(path IN LISTS ARGN)
        file(STRINGS "${path}" namespace_lines
            REGEX "^[ \t]*namespace[ \t]+[A-Za-z_][A-Za-z0-9_:]*[ \t]*\\{")
        foreach(line IN LISTS namespace_lines)
            string(REGEX MATCH
                "namespace[ \t]+([A-Za-z_][A-Za-z0-9_:]*)[ \t]*\\{"
                namespace_match "${line}")
            if(NOT namespace_match)
                continue()
            endif()
            set(namespace_name "${CMAKE_MATCH_1}")
            if(NOT namespace_name MATCHES "^Aero(::|$)")
                continue()
            endif()
            set(namespace_allowed FALSE)
            foreach(prefix IN LISTS
                    AERO_PUBLIC_NAMESPACE_PREFIXES
                    AERO_PUBLIC_NAMESPACE_DETAIL_PREFIXES)
                if(namespace_name STREQUAL prefix OR
                   namespace_name MATCHES "^${prefix}::")
                    set(namespace_allowed TRUE)
                    break()
                endif()
            endforeach()
            if(NOT namespace_allowed)
                file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
                list(APPEND matches "${relative}: ${namespace_name}")
            endif()
        endforeach()
    endforeach()
    set(${output} "${matches}" PARENT_SCOPE)
endfunction()

set(aero_namespace_headers)
foreach(relative IN LISTS AERO_PUBLIC_HEADERS)
    list(APPEND aero_namespace_headers "${AERO_SOURCE_DIR}/${relative}")
endforeach()

aero_collect_matches(public_using_namespace
    "using[ \t]+namespace[ \t]+"
    ${aero_namespace_headers})
if(public_using_namespace)
    message(FATAL_ERROR
        "Installed headers must not inject namespaces with using namespace: "
        "${public_using_namespace}")
endif()

aero_collect_public_namespace_declarations(public_namespace_declarations
    ${aero_namespace_headers})
if(public_namespace_declarations)
    message(FATAL_ERROR
        "Public namespace declaration is outside the canonical namespace manifest: "
        "${public_namespace_declarations}")
endif()

# This is a non-negotiable SDK boundary. Keep the exact spellings here in
# addition to the namespace manifest so a future manifest edit cannot weaken
# the check accidentally.
set(public_internal_namespace_patterns
    "namespace[ \t]+Internal([ \t:{]|$)"
    "namespace[ \t]+Aero::Internal([ \t:{]|$)"
    "Aero::Internal::"
    "::Internal::")
set(public_internal_namespace_matches)
foreach(pattern IN LISTS public_internal_namespace_patterns)
    aero_collect_matches(matches "${pattern}" ${aero_namespace_headers})
    list(APPEND public_internal_namespace_matches ${matches})
endforeach()
if(public_internal_namespace_matches)
    list(REMOVE_DUPLICATES public_internal_namespace_matches)
    message(FATAL_ERROR
        "Public headers expose Aero::Internal or namespace Internal: "
        "${public_internal_namespace_matches}")
endif()

set(public_internal_type_pattern
    "(^|[^A-Za-z0-9_])(ElementPrivate|ControlPrivate|TemplatePrivate|StylePrivate|TransformPrivate|BrushPrivate|EffectPrivate|AnimationPrivate|DesktopPrivate|ViewData|RenderTree|RenderResources|RenderFunctions|RenderDeviceFactory|AdoptRenderDevice|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RoutedHandlerStorage|RoutedHandlerTraits|RuntimeManagersFwd|ThemeStyleRegistry)([^A-Za-z0-9_]|$)")
aero_collect_matches(public_internal_type_matches
    "${public_internal_type_pattern}"
    ${aero_namespace_headers})
if(public_internal_type_matches)
    message(FATAL_ERROR
        "Public headers expose an implementation-only type or gateway: "
        "${public_internal_type_matches}")
endif()

if(EXISTS "${AERO_SOURCE_DIR}/include/Aero/Renderer.hpp" OR
   EXISTS "${AERO_SOURCE_DIR}/include/Aero/Integration/RenderDevice.hpp")
    message(FATAL_ERROR
        "Retired rendering facade headers must not exist; use "
        "<Aero/IRenderer.hpp> and <Aero/RenderDevice.hpp>")
endif()
aero_collect_matches(public_legacy_render_device
    "Integration::RenderDevice|class[ \\t]+Renderer([ \\t:{]|$)"
    ${aero_namespace_headers})
if(public_legacy_render_device)
    message(FATAL_ERROR
        "Installed headers expose a retired renderer/device spelling: "
        "${public_legacy_render_device}")
endif()

set(aero_public_render_device_header
    "${AERO_SOURCE_DIR}/include/Aero/RenderDevice.hpp")
aero_collect_matches(public_render_surface_leaks
    "RenderDeviceMode|RenderPresentMode|NotifySurfaceLost|Resize[ \t\r\n]*[(]"
    "${aero_public_render_device_header}")
if(public_render_surface_leaks)
    message(FATAL_ERROR
        "RenderDevice must not expose presentation or surface lifecycle: "
        "${public_render_surface_leaks}")
endif()
unset(aero_public_render_device_header)
unset(public_render_surface_leaks)

# Backend-specific rendering files are shader catalogs only. Surface acquire,
# target import, submission and presentation belong to Integration; the only
# active renderer implementation is Render::DeviceRenderer.
set(aero_retired_native_renderer_headers
    "${AERO_SOURCE_DIR}/src/render/d3d11/D3D11Renderer.hpp"
    "${AERO_SOURCE_DIR}/src/render/opengl33/OpenGL33Renderer.hpp")
foreach(header IN LISTS aero_retired_native_renderer_headers)
    if(EXISTS "${header}")
        message(FATAL_ERROR
            "Retired native Renderer header must not exist: ${header}")
    endif()
endforeach()
set(aero_shader_catalog_headers
    "${AERO_SOURCE_DIR}/src/render/d3d11/D3D11Shaders.hpp"
    "${AERO_SOURCE_DIR}/src/render/opengl33/OpenGL33Shaders.hpp")
foreach(header IN LISTS aero_shader_catalog_headers)
    if(NOT EXISTS "${header}")
        message(FATAL_ERROR
            "Native shader catalog header is missing: ${header}")
    endif()
endforeach()
file(GLOB_RECURSE aero_native_renderer_reference_scan
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp")
aero_collect_matches(retired_native_renderer_header_references
    "D3D11Renderer[.]hpp|OpenGL33Renderer[.]hpp"
    ${aero_native_renderer_reference_scan})
if(retired_native_renderer_header_references)
    message(FATAL_ERROR
        "Retired native Renderer header reference remains: "
        "${retired_native_renderer_header_references}")
endif()
set(aero_renderer_boundary_scan
    "${AERO_SOURCE_DIR}/src/render/FrameEncoder.hpp"
    "${AERO_SOURCE_DIR}/src/integration/IntegrationPrivate.hpp"
    "${AERO_SOURCE_DIR}/src/integration/D3D11Device.cpp"
    "${AERO_SOURCE_DIR}/src/integration/OpenGL33Device.cpp"
    "${AERO_SOURCE_DIR}/src/integration/RenderSurface.cpp"
    "${AERO_SOURCE_DIR}/tools/conformance/main.cpp")
aero_collect_matches(retired_native_renderer_types
    "D3D11Renderer|OpenGL33Renderer|D3D11EmbeddedDeviceOptions|D3D11WindowDeviceOptions|OpenGL33EmbeddedDeviceOptions|OpenGL33WindowDeviceOptions"
    ${aero_renderer_boundary_scan})
if(retired_native_renderer_types)
    message(FATAL_ERROR
        "Retired native Renderer or device-option alias remains active: "
        "${retired_native_renderer_types}")
endif()
aero_collect_matches(private_rendering_aliases
    "using[ \t]+(RenderDevice|RenderDeviceMode|RenderPresentMode|RenderDeviceState|RenderDeviceStatistics|RenderFrameStatistics)[ \t]*="
    "${AERO_SOURCE_DIR}/src/integration/IntegrationPrivate.hpp")
if(private_rendering_aliases)
    message(FATAL_ERROR
        "IntegrationPrivate must not re-export rendering type aliases: "
        "${private_rendering_aliases}")
endif()
unset(aero_retired_native_renderer_headers)
unset(aero_shader_catalog_headers)
unset(aero_native_renderer_reference_scan)
unset(retired_native_renderer_header_references)
unset(aero_renderer_boundary_scan)
unset(retired_native_renderer_types)
unset(private_rendering_aliases)

set(aero_render_device_private_header
    "${AERO_SOURCE_DIR}/src/integration/private/RenderDevice.hpp")
set(aero_render_surface_private_header
    "${AERO_SOURCE_DIR}/src/integration/private/RenderSurface.hpp")
aero_collect_matches(render_device_surface_gateway_leaks
    "ResizeSurface|NotifySurfaceLost|RestoreSurface|SurfaceState|SurfaceStatus|Base::Result<void>[ \t\r\n]+[(][*]render[)]|[(][*]resize[)]|[(][*]surfaceLost[)]"
    "${aero_render_device_private_header}")
if(render_device_surface_gateway_leaks)
    message(FATAL_ERROR
        "RenderDevice private contract still owns surface operations: "
        "${render_device_surface_gateway_leaks}")
endif()
aero_collect_matches(render_surface_contract_missing
    "struct[ \t]+RenderSurfaceFunctions|restoreSurface|SurfaceHealth"
    "${aero_render_surface_private_header}")
if(NOT render_surface_contract_missing)
    message(FATAL_ERROR
        "RenderSurface private contract does not own its lifecycle functions")
endif()
unset(aero_render_device_private_header)
unset(aero_render_surface_private_header)
unset(render_device_surface_gateway_leaks)
unset(render_surface_contract_missing)

set(aero_public_style_headers
    "${AERO_SOURCE_DIR}/include/Aero/Style.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Styling.hpp")
file(GLOB aero_public_trigger_headers
    "${AERO_SOURCE_DIR}/include/Aero/Triggers/*.hpp")
aero_collect_matches(public_style_plan_matches
    "(^|[^A-Za-z0-9_])(StyleSetter|StyleTriggerSetter|TriggerPlan|StyleProgram|TemplateProgram)([^A-Za-z0-9_]|$)"
    ${aero_public_style_headers}
    ${aero_public_trigger_headers})
if(public_style_plan_matches)
    message(FATAL_ERROR
        "Public Style/Trigger authoring headers expose runtime plans or programs: "
        "${public_style_plan_matches}")
endif()
unset(aero_public_style_headers)
unset(aero_public_trigger_headers)
unset(public_style_plan_matches)

if(EXISTS "${AERO_SOURCE_DIR}/include/Aero/RoutedEvent.hpp" OR
   EXISTS "${AERO_SOURCE_DIR}/include/Aero/Events/Events.hpp")
    message(FATAL_ERROR
        "Retired event facade headers must not exist; use <Aero/Events.hpp> "
        "and <Aero/Events/RoutedEvent.hpp>")
endif()
file(GLOB_RECURSE aero_retired_event_path_scan
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/docs/*.md"
    "${AERO_SOURCE_DIR}/docs/*.txt"
    "${AERO_SOURCE_DIR}/README.md"
    "${AERO_SOURCE_DIR}/.github/*.yml"
    "${AERO_SOURCE_DIR}/.github/*.yaml")
set(retired_event_path_matches)
foreach(pattern IN ITEMS
        "#[ \t]*include[ \t]*<Aero/RoutedEvent[.]hpp>"
        "#[ \t]*include[ \t]*<Aero/Events/Events[.]hpp>")
    aero_collect_matches(matches "${pattern}" ${aero_retired_event_path_scan})
    list(APPEND retired_event_path_matches ${matches})
endforeach()
if(retired_event_path_matches)
    message(FATAL_ERROR
        "Retired event header path remains referenced: "
        "${retired_event_path_matches}")
endif()
unset(public_internal_namespace_patterns)
unset(public_internal_namespace_matches)
unset(public_internal_type_pattern)
unset(public_internal_type_matches)
unset(aero_retired_event_path_scan)
unset(retired_event_path_matches)

# Public/XAML enum descriptions have one canonical owner.  The declarations
# in SDK headers only provide a runtime C++ type token; names, TypeIds and
# value tables belong to src/gui/EnumMetadata.cpp.
set(aero_xaml_enum_names
    ShutdownMode WindowState WindowStyle ResizeMode SizeToContent
    InputScope KeyboardNavigationMode FillBehavior EasingMode
    ControlStoryboardOption ComparisonConditionOperator ForwardChaining
    HorizontalAlignment VerticalAlignment Visibility BlendMode Stretch
    StretchDirection TileMode BrushMappingMode GradientSpreadMethod
    PenLineJoin PenLineCap
    TextWrapping TextTrimming TextAlignment FontStyle FontWeight
    TextDecorations Orientation Dock MenuItemRole ClickMode TickPlacement
    TickBarPlacement ScrollBarVisibility PanningMode GridResizeDirection
    GridResizeBehavior SelectionMode ExpandDirection PlacementMode
    PopupAnimation GridViewColumnHeaderRole ScrollUnit VirtualizationMode)
string(JOIN "|" aero_xaml_enum_name_pattern ${aero_xaml_enum_names})
set(aero_enum_trait_pattern
    "template[ \\t\\r\\n]*<[ \\t]*>[ \\t\\r\\n]*struct[ \\t]+TypeTraits[ \\t]*<[^>]*(${aero_xaml_enum_name_pattern})")
aero_collect_matches(public_enum_trait_violations
    "${aero_enum_trait_pattern}"
    ${aero_namespace_headers})
if(public_enum_trait_violations)
    message(FATAL_ERROR
        "Public enum TypeTraits descriptions must be declared with "
        "AERO_DECLARE_TYPE_ENUM, not defined inline: "
        "${public_enum_trait_violations}")
endif()

set(enum_metadata_file "${AERO_SOURCE_DIR}/src/gui/EnumMetadata.cpp")
if(NOT EXISTS "${enum_metadata_file}")
    message(FATAL_ERROR
        "Central enum metadata owner is missing: src/gui/EnumMetadata.cpp")
endif()
file(READ "${enum_metadata_file}" enum_metadata_content)
if(NOT enum_metadata_content MATCHES "Meta::Register" OR
   NOT enum_metadata_content MATCHES "PopulateEnumMetadata")
    message(FATAL_ERROR
        "src/gui/EnumMetadata.cpp must register enum descriptions through Meta::Register")
endif()
set(aero_enum_metadata_names
    ShutdownMode WindowState WindowStyle ResizeMode SizeToContent
    InputScope KeyboardNavigationMode FillBehavior EasingMode
    ControlStoryboardOption ComparisonConditionOperator ForwardChaining
    HorizontalAlignment VerticalAlignment Visibility BlendMode Stretch
    StretchDirection TileMode BrushMappingMode GradientSpreadMethod
    PenLineJoin PenLineCap
    TextWrapping TextTrimming TextAlignment FontStyle FontWeight
    TextDecorations Orientation Dock MenuItemRole ClickMode TickPlacement
    TickBarPlacement ScrollBarVisibility PanningMode GridResizeDirection
    GridResizeBehavior SelectionMode ExpandDirection PlacementMode
    PopupAnimation GridViewColumnHeaderRole ScrollUnit VirtualizationMode)
set(enum_metadata_missing_names)
foreach(enum_name IN LISTS aero_enum_metadata_names)
    if(NOT enum_metadata_content MATCHES "\\\"${enum_name}\\\"")
        list(APPEND enum_metadata_missing_names "${enum_name}")
    endif()
endforeach()
if(enum_metadata_missing_names)
    message(FATAL_ERROR
        "Central enum metadata is missing XAML names: "
        "${enum_metadata_missing_names}")
endif()

file(GLOB_RECURSE enum_registration_sources
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inl"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tests/*.cpp"
    "${AERO_SOURCE_DIR}/tests/*.hpp")
aero_collect_matches(enum_registration_matches
    "Meta::Register[ \\t\\r\\n]*<[^>]*(${aero_xaml_enum_name_pattern})[ \\t\\r\\n>]"
    ${enum_registration_sources})
set(enum_registration_violations)
foreach(relative IN LISTS enum_registration_matches)
    if(NOT relative STREQUAL "src/gui/EnumMetadata.cpp")
        list(APPEND enum_registration_violations "${relative}")
    endif()
endforeach()
if(enum_registration_violations)
    list(REMOVE_DUPLICATES enum_registration_violations)
    message(FATAL_ERROR
        "Public/XAML enum registration must be centralized in "
        "src/gui/EnumMetadata.cpp: ${enum_registration_violations}")
endif()

string(CONCAT removed_enum_registration_macro_pattern
    "AERO_DEFINE_TYPE_" "ENUM" "|"
    "AERO_WINDOW_ENUM_" "TRAITS")
aero_collect_matches(removed_enum_registration_macros
    "${removed_enum_registration_macro_pattern}"
    ${enum_registration_sources} ${aero_namespace_headers})
if(removed_enum_registration_macros)
    message(FATAL_ERROR
        "Removed enum registration macros remain: "
        "${removed_enum_registration_macros}")
endif()
unset(enum_metadata_file)
unset(enum_metadata_content)
unset(aero_enum_trait_pattern)
unset(aero_enum_metadata_names)
unset(aero_xaml_enum_names)
unset(aero_xaml_enum_name_pattern)
unset(public_enum_trait_violations)
unset(enum_metadata_missing_names)
unset(enum_registration_sources)
unset(enum_registration_matches)
unset(enum_registration_violations)
unset(removed_enum_registration_macros)

set(public_forbidden_namespace_matches)
foreach(forbidden_pattern IN LISTS AERO_PUBLIC_NAMESPACE_FORBIDDEN_PATTERNS)
    aero_collect_matches(forbidden_matches
        "${forbidden_pattern}"
        ${aero_namespace_headers})
    list(APPEND public_forbidden_namespace_matches ${forbidden_matches})
endforeach()
if(public_forbidden_namespace_matches)
    list(REMOVE_DUPLICATES public_forbidden_namespace_matches)
    message(FATAL_ERROR
        "Public headers expose a forbidden Render or product Detail namespace: "
        "${public_forbidden_namespace_matches}")
endif()

# Detail is an implementation seam, not a general-purpose public namespace.
# Base detail declarations are temporarily allowed only in the explicit
# headers that own the corresponding ABI-facing forward declarations.  Keep
# this allow-list path based so a new public header cannot accidentally expose
# implementation state merely by adding a friend or a qualified type name.
aero_collect_matches(public_base_detail_matches
    "namespace[ \\t]+Detail[ \\t]*\\{"
    ${aero_namespace_headers})
set(public_detail_matches
    ${public_base_detail_matches})
list(REMOVE_DUPLICATES public_detail_matches)
set(public_detail_allowlist
    ${AERO_PUBLIC_NAMESPACE_DETAIL_HEADERS}
    ${AERO_PUBLIC_NAMESPACE_BASE_DETAIL_HEADERS})
set(public_unlisted_detail_matches)
foreach(relative IN LISTS public_detail_matches)
    list(FIND public_detail_allowlist "${relative}" detail_index)
    if(detail_index EQUAL -1)
        list(APPEND public_unlisted_detail_matches "${relative}")
    endif()
endforeach()
if(public_unlisted_detail_matches)
    message(FATAL_ERROR
        "Base Detail references are not listed in the public namespace manifest: "
        "${public_unlisted_detail_matches}")
endif()
unset(aero_namespace_headers)
unset(public_forbidden_namespace_matches)
unset(public_base_detail_matches)
unset(public_detail_matches)
unset(public_detail_allowlist)
unset(public_unlisted_detail_matches)

# Core was removed as an SDK namespace.  Keep the retired spelling rejected
# across source, tests, tools and documentation so it cannot reappear through
# a stale include or generated consumer.
file(GLOB_RECURSE aero_core_retired_scan
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h"
    "${AERO_SOURCE_DIR}/src/*"
    "${AERO_SOURCE_DIR}/tests/*"
    "${AERO_SOURCE_DIR}/samples/*"
    "${AERO_SOURCE_DIR}/tools/*"
    "${AERO_SOURCE_DIR}/docs/*")
aero_collect_matches(retired_core_namespace_matches
    "Aero:[ \t]*:[ \t]*Core|namespace[ \t]+Core|Core:[ \t]*:|<Aero[/]Core/"
    ${aero_core_retired_scan})
if(retired_core_namespace_matches)
    message(FATAL_ERROR
        "Retired core namespace spelling remains in the repository: "
        "${retired_core_namespace_matches}")
endif()
unset(aero_core_retired_scan)
unset(retired_core_namespace_matches)

file(GLOB_RECURSE core_files
    "${AERO_SOURCE_DIR}/src/gui/*.cpp"
    "${AERO_SOURCE_DIR}/src/gui/*.hpp")
list(FILTER core_files EXCLUDE REGEX "/EnumMetadata\\.cpp$")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/include/Aero/Meta.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Value.hpp"
    "${AERO_SOURCE_DIR}/src/diagnostics/Diagnostics.cpp"
    "${AERO_SOURCE_DIR}/src/gui/Dispatcher.cpp"
    "${AERO_SOURCE_DIR}/src/gui/ObjectFactory.cpp"
    "${AERO_SOURCE_DIR}/src/gui/GuiPrivate.hpp")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/include/Aero/DependencyProperty.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Diagnostics.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Diagnostics/PropertyValueSource.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Events/RoutedEvent.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Threading.hpp")
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
    "#[ \t]*include[ \t]*<Aero/(Controls|Markup|Integration|DependencyProperty|RoutedEvent|Meta/)/"
    ${text_files})
if(text_reverse)
    message(FATAL_ERROR
        "Text provider contracts must depend only on Base: ${text_reverse}")
endif()

file(GLOB_RECURSE rhi_public_files
    "${AERO_SOURCE_DIR}/include/Aero/Rhi/*.hpp")
aero_collect_matches(rhi_reverse
    "#[ \t]*include[ \t]*<Aero/(Controls|Markup|Render|Text)/"
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
    "src/gui/LegacyActivation.hpp"
    "src/gui/MetadataDescriptors.cpp"
    "src/markup/LoaderEngine.hpp"
    "src/markup/LoaderEngine.cpp"
    "include/Aero/Markup/RuntimeHost.hpp"
    "include/Aero/Markup/XamlThemeResources.hpp"
    "include/Aero/Markup/XamlModuleSdk.hpp"
    "include/Aero/RuntimeServices.hpp"
    "src/markup/RuntimeHost.inc"
    "src/markup/RuntimeWindow.inc"
    "src/markup/Invariants.inc"
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
    "include/Aero/Integration/HostedGraphics.hpp"
    "include/Aero/Integration/HostServices.hpp"
    "include/Aero/Integration/View.hpp"
    "include/Aero/Integration/ViewHost.hpp"
    "include/Aero/App/Launcher.hpp"
    "include/Aero/App/ApplicationHost.hpp"
    "include/Aero/App/Services.hpp"
    "include/Aero/App/Fwd.hpp"
    "include/Aero/App/Application.hpp"
    "include/Aero/App/Window.hpp"
    "include/Aero/ModuleSdk.hpp"
    "include/Aero/Metadata.hpp"
    "include/Aero/Detail/UiMetadata.hpp"
    "include/Aero/RuntimeHost.hpp"
    "include/Aero/XamlReloadCoordinator.hpp"
    "include/Aero/BuiltinModules.hpp"
    "include/Aero/GuiSchema.hpp"
    "include/Aero/Markup/Loader.hpp"
    "include/Aero/Markup/Extensions.hpp"
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
    "include/Aero/ElementTree.hpp"
    "include/Aero/Rendering.hpp"
    "include/Aero/Data/Binding.hpp"
    "include/Aero/Documents/Documents.hpp"
    "include/Aero/Input/Commands.hpp"
    "include/Aero/Input/Navigation.hpp"
    "include/Aero/Input/Values.hpp"
    "include/Aero/Media.hpp"
    "include/Aero/Media/Animation.hpp"
    "include/Aero/Text/Text.hpp"
    "include/Aero/Platform/Win32Window.hpp"
    "include/Aero/Platform/X11Window.hpp"
    "include/Aero/Invariants.hpp"
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
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
aero_collect_matches(legacy_markup_includes
    "${legacy_markup_header_pattern}" ${production_code})
if(legacy_markup_includes)
    message(FATAL_ERROR
        "Production code must not include removed Markup headers: "
        "${legacy_markup_includes}")
endif()

file(GLOB_RECURSE current_code
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inc"
    "${AERO_SOURCE_DIR}/tests/*.cpp"
    "${AERO_SOURCE_DIR}/tests/*.inc"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
set(markup_kernel_files
    "${AERO_SOURCE_DIR}/src/markup/MarkupParser.cpp")
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
    "${AERO_SOURCE_DIR}/include/Aero/View.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/ViewOptions.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/RenderDevice.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/Providers/Providers.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/Providers/XamlProvider.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/Providers/FontProvider.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/Providers/TextureProvider.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Events.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Triggers/Triggers.hpp")
aero_collect_matches(sdk_entry_leaks
    "(RuntimeHost|RenderPlan|IRenderBackend|GraphicsDevice|SurfaceSession|Presenter|[A-Za-z]+Manager|[A-Za-z]+Store|[A-Za-z]+Program|DocumentCache|TransactionCallback)"
    ${sdk_entry_headers})
if(sdk_entry_leaks)
    message(FATAL_ERROR
        "Default SDK headers expose runtime implementation types: ${sdk_entry_leaks}")
endif()

aero_collect_matches(retired_view_host_surface
    "(ViewHost|ViewHostOptions|Aero/Integration/View[.]hpp)"
    ${sdk_entry_headers})
if(retired_view_host_surface)
    message(FATAL_ERROR
        "The forwarding ViewHost layer or old View header path remains: "
        "${retired_view_host_surface}")
endif()

set(platform_service_contract
    "${AERO_SOURCE_DIR}/include/Aero/Integration/Platform.hpp")
aero_collect_matches(public_native_platform_adapters
    "(Win32|X11|DispatchWin32|WindowMessage|GetActiveWindow|HWND|HIMC)"
    ${platform_service_contract})
if(public_native_platform_adapters)
    message(FATAL_ERROR
        "PlatformServices must expose contracts only; native adapters are private: "
        "${public_native_platform_adapters}")
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


aero_collect_matches(default_property_diagnostics
    "using[ \t]+(PropertyValueRank|PropertyValueSourceInfo)[ \t]*="
    "${AERO_SOURCE_DIR}/include/Aero/DependencyObject.hpp")
if(default_property_diagnostics)
    message(FATAL_ERROR
        "Dependency-property provider diagnostics must remain opt-in: "
        "${default_property_diagnostics}")
endif()

file(GLOB_RECURSE default_sdk_headers
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")

aero_collect_matches(removed_public_services
    "(RoutedEventTable|DescriptionBuilder|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RenderCommand|RenderImageId|RenderMeshId|RenderGlyphRunId|ThemeStyleRegistry|PPAAOutProperty|PasswordLengthProperty|RuntimeManagersFwd|RoutedHandlerStorage|RoutedHandlerTraits|Aero/Detail/|BuildEditorDisplayList|RuntimeAnimation\\(|RuntimeFrame\\(|RuntimeEasing\\(|ItemContainerGeneratorImpl[ \t]*[*]|VisualStateManagerImpl[ \t]*[*])"
    ${default_sdk_headers})
if(removed_public_services)
    message(FATAL_ERROR
        "Removed manager, metadata detail, or text service leaks through public headers: "
        "${removed_public_services}")
endif()


set(routed_event_headers
    "${AERO_SOURCE_DIR}/include/Aero/Events/RoutedEvent.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/UIElement.hpp")
aero_collect_matches(const_routed_event_handlers
    "Delegate<void[(]Base::Object[*],[ \t]*const[ \t]+[A-Za-z0-9_:]+EventArgs[&][)]>"
    ${routed_event_headers})
if(const_routed_event_handlers)
    message(FATAL_ERROR
        "Routed event handlers must receive mutable event arguments: "
        "${const_routed_event_handlers}")
endif()

file(GLOB_RECURSE event_route_sources
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_matches(duplicate_event_routes
    "BuildEventRoute|SnapshotRoute"
    ${event_route_sources})
if(duplicate_event_routes)
    message(FATAL_ERROR
        "Input and commands must use the canonical EventRoute: "
        "${duplicate_event_routes}")
endif()

aero_collect_matches(split_view_input_services
    "Aero::Detail::(FocusManager|PointerInputManager|KeyboardInputManager|TextInputManager)[*][ \t]+(focus|pointer|keyboard|textInput)"
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp")
if(split_view_input_services)
    message(FATAL_ERROR
        "View must own input through its private InputRouter aggregate: "
        "${split_view_input_services}")
endif()

aero_collect_matches(command_parent_walk
    "Get(Visual|Logical)Parent[ \t]*[(]"
    "${AERO_SOURCE_DIR}/src/gui/Commands.cpp")
if(command_parent_walk)
    message(FATAL_ERROR
        "Command routing must consume the canonical EventRoute instead of walking parents: "
        "${command_parent_walk}")
endif()

set(control_authoring_headers
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Core.hpp")
aero_collect_matches(public_template_runtime_surface
    "(DefaultStyleKey|TemplateGeneration|(^|[ \t])TemplateChild[ \t]*[(]|(^|[ \t])SetTemplateChild[ \t]*[(]|(^|[ \t])IsTemplateApplied[ \t]*[(])"
    ${control_authoring_headers})
if(public_template_runtime_surface)
    message(FATAL_ERROR
        "Control authoring headers expose template runtime state: "
        "${public_template_runtime_surface}")
endif()

aero_collect_matches(legacy_ui_element_collection_surface
    "((^|[ \t])std::uint32_t[ \t]+Count[ \t]*[(]|(^|[ \t])UIElement[*][ \t]+At[ \t]*[(])"
    ${control_authoring_headers})
if(legacy_ui_element_collection_surface)
    message(FATAL_ERROR
        "UIElementCollection must use GetCount/GetItem/GetIsEmpty: "
        "${legacy_ui_element_collection_surface}")
endif()

set(template_authoring_headers
    "${AERO_SOURCE_DIR}/include/Aero/Styling.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Items.hpp")
aero_collect_matches(public_template_program_surface
    "(TemplateBuilder|TemplateFactoryCallback|TemplateBindingPlan|TemplateMetadataBindingPlan|TemplateTriggerSetter|TemplateTriggerCondition|TemplatePropertyTrigger|DeferredObjectFactory|DeferredObjectProgram|RuntimeData|FactoryContext|AuthoredVisualTree|AuthoredVisualStateGroups|AuthoredNames|SealRuntime)"
    ${template_authoring_headers})
if(public_template_program_surface)
    message(FATAL_ERROR
        "Template authoring headers expose compiler or runtime program details: "
        "${public_template_program_surface}")
endif()

aero_collect_matches(public_visual_state_runtime_surface
    "(GoToStateCore|ClearStateCore|ClearCore|CurrentStateCore|VisualStateManagerImpl[ \t]*[*])"
    "${AERO_SOURCE_DIR}/include/Aero/Styling.hpp")
if(public_visual_state_runtime_surface)
    message(FATAL_ERROR
        "VisualStateManager must remain a static WPF-facing facade: "
        "${public_visual_state_runtime_surface}")
endif()

set(control_runtime_attachment_headers
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Core.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Items.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Panels.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Primitives.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Common.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Text.hpp")
aero_collect_matches(typed_control_runtime_attachments
    "(ButtonBehavior|MenuBehavior|ScrollBehavior|ListBehavior|ComboBehavior|TreeBehavior)[ \t]*[*]"
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
    "add_library\\([ \t\r\n]*Aero::(Rhi|Render|ModuleSet)([A-Za-z0-9_]|[ \t\r\n])"
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


# G-series product and rendering convergence gates.
if(EXISTS "${AERO_SOURCE_DIR}/src/rhi")
    message(FATAL_ERROR
        "The retired src/rhi layer must not be recreated; use src/render")
endif()
if(EXISTS "${AERO_SOURCE_DIR}/src/graphics")
    message(FATAL_ERROR
        "GPU device and backend implementation belongs to src/render; "
        "the split src/graphics layer must not be recreated")
endif()
if(EXISTS "${AERO_SOURCE_DIR}/cmake/AeroGraphicsTargets.cmake")
    message(FATAL_ERROR
        "Rendering targets must be composed by AeroRenderingTargets.cmake")
endif()

file(GLOB_RECURSE converged_runtime_sources
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_matches(retired_render_graphics_types
    "(namespace[ \t]+Aero::Rhi|RhiDevice|IGraphicsBackend|IRenderBackend|EndpointDriver|QueuedRenderBackend|RenderPlan|RenderManager|(^|[^A-Za-z0-9_])GraphicsCommand(Buffer|Encoder|Kind)?([^A-Za-z0-9_]|$))"
    ${converged_runtime_sources})
if(retired_render_graphics_types)
    message(FATAL_ERROR
        "Retired render/RHI layers or aliases remain: "
        "${retired_render_graphics_types}")
endif()

file(GLOB aero_target_modules
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/cmake/Aero*Targets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroInstall.cmake")

# Internal source domains are build-only object components. They must never
# acquire public aliases or reappear as installable support binaries.
aero_collect_matches(public_internal_target_aliases
    "add_library\\([ \\t\\r\\n]*Aero::(_Detail|GuiKernel|Text|TextFreeType|TextHarfBuzz|Controls|Inspector|MarkupKernel|Markup|AppModel|ModuleSet|Runtime|Rendering)"
    ${aero_target_modules})
if(public_internal_target_aliases)
    message(FATAL_ERROR
        "Internal object components must not expose CMake aliases: "
        "${public_internal_target_aliases}")
endif()

aero_collect_matches(internal_support_binaries
    "add_library\\([ \\t\\r\\n]*(AeroGuiKernel|AeroText|AeroTextFreeType|AeroTextHarfBuzz|AeroControls|AeroInspector|AeroMarkupKernel|AeroMarkup|AeroAppModel|AeroModuleSet|AeroRuntime|AeroRendering)[ \\t\\r\\n]+(STATIC|SHARED|MODULE|\\$\\{AERO_LIBRARY_TYPE\\})"
    ${aero_target_modules})
if(internal_support_binaries)
    message(FATAL_ERROR
        "Internal Aero domains must compile as object components, not binaries: "
        "${internal_support_binaries}")
endif()

aero_collect_matches(split_render_targets
    "add_library[^A-Za-z0-9_:]+(Aero(Graphics(OpenGL33|D3D11)?|Render(OpenGL33|D3D11)?|Platform(WGL|GLX))|Aero::_Detail(Graphics(OpenGL33|D3D11)?|Render(OpenGL33|D3D11)?|Platform(WGL|GLX)))[ \\t\\r\\n]"
    ${aero_target_modules})
if(split_render_targets)
    message(FATAL_ERROR
        "Renderer, RenderDevice and native backends must use the single "
        "AeroRenderingObjects component: ${split_render_targets}")
endif()

file(READ "${AERO_SOURCE_DIR}/cmake/AeroInstall.cmake"
    aero_install_content)
if(aero_install_content MATCHES "EXPORT_NAME[ \\t\\r\\n]+_Detail")
    message(FATAL_ERROR
        "Installed packages must not export _Detail implementation targets")
endif()
if(aero_install_content MATCHES
        "(^|[^A-Za-z0-9_])(AeroGuiKernel|AeroText(FreeType|HarfBuzz)?|AeroControls|AeroInspector|AeroMarkup(Kernel)?|AeroAppModel|AeroModuleSet|AeroRuntime|AeroRendering)([^A-Za-z0-9_]|$)")
    message(FATAL_ERROR
        "AeroInstall.cmake must install only product targets and private "
        "third-party archives")
endif()
unset(aero_install_content)

foreach(required_object_target IN ITEMS
        AeroGuiKernelObjects
        AeroTextObjects
        AeroControlsObjects
        AeroMarkupKernelObjects
        AeroMarkupObjects
        AeroAppModelObjects
        AeroModuleSetObjects
        AeroTextFreeTypeObjects
        AeroTextHarfBuzzObjects
        AeroRuntimeObjects
        AeroRenderingObjects)
    set(required_object_definition)
    aero_collect_matches(required_object_definition
        "add_library\\([ \\t\\r\\n]*${required_object_target}[ \\t\\r\\n]+OBJECT"
        ${aero_target_modules})
    if(NOT required_object_definition)
        message(FATAL_ERROR
            "Required internal object component is missing: "
            "${required_object_target}")
    endif()
endforeach()
unset(required_object_target)
unset(required_object_definition)

file(READ "${AERO_SOURCE_DIR}/cmake/AeroGuiTargets.cmake"
    aero_gui_target_content)
if(aero_gui_target_content MATCHES
        "add_library\\([ \\t\\r\\n]*AeroGui[ \\t\\r\\n]+INTERFACE")
    message(FATAL_ERROR
        "Aero::Gui must be a real product binary, not an interface route")
endif()
unset(aero_gui_target_content)

# H-series source ownership and runtime convergence gates.
file(GLOB aero_root_source_files
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
if(aero_root_source_files)
    message(FATAL_ERROR
        "Source files must belong to a domain directory under src/: "
        "${aero_root_source_files}")
endif()

foreach(required_private_header IN ITEMS
        "src/gui/GuiPrivate.hpp"
        "src/controls/ControlsPrivate.hpp"
        "src/markup/MarkupPrivate.hpp"
        "src/media/MediaPrivate.hpp"
        "src/render/RenderPrivate.hpp"
        "src/integration/IntegrationPrivate.hpp"
        "src/controls/TemplateProgram.hpp"
        "src/controls/TemplateInstance.hpp"
        "src/platform/win32/InputRouters.hpp"
        "src/platform/win32/Window.hpp"
        "src/platform/x11/Window.hpp")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${required_private_header}")
        message(FATAL_ERROR
            "Required converged private header is missing: ${required_private_header}")
    endif()
endforeach()

foreach(retired_private_file IN ITEMS
        "src/gui/AnimationInternal.hpp"
        "src/gui/BindingInternal.hpp"
        "src/gui/ElementInternal.hpp"
        "src/gui/InputInternal.hpp"
        "src/gui/LayoutInternal.hpp"
        "src/gui/MetadataInternal.hpp"
        "src/gui/PropertyInternal.hpp"
        "src/gui/RoutedEventInternal.hpp"
        "src/gui/StyleInternal.hpp"
        "src/controls/ControlInternals.hpp"
        "src/controls/ItemsInternal.hpp"
        "src/controls/TemplateInternals.hpp"
        "src/markup/MarkupInternal.hpp"
        "src/markup/MarkupWriterInternal.hpp"
        "src/media/AnimationInternals.hpp"
        "src/media/BrushInternals.hpp"
        "src/media/EffectInternals.hpp"
        "src/media/TransformInternals.hpp"
        "src/render/DrawingInternals.hpp"
        "src/integration/RenderDeviceInternal.hpp"
        "src/gui/ControlBehavior.hpp"
        "src/gui/RuntimeServices.hpp"
        "src/controls/TemplateRuntime.hpp"
        "src/render/TextBackendAccess.hpp"
        "src/render/Device.hpp"
        "src/graphics/Device.hpp"
        "src/runtime/PresentationRuntime.cpp"
        "src/runtime/PresentationRuntime.hpp"
        "src/runtime/RuntimeFwd.hpp"
        "src/runtime/ViewRuntime.cpp"
        "src/runtime/ViewRuntime.hpp"
        "src/runtime/RuntimeUiServices.cpp"
        "src/runtime/RuntimeUiServices.hpp"
        "src/platform/Ime.cpp"
        "src/platform/Win32Window.cpp"
        "src/platform/Win32Window.hpp"
        "src/platform/X11Window.cpp"
        "src/platform/X11Window.hpp"
        "cmake/AeroPlatformSources.cmake")
    if(EXISTS "${AERO_SOURCE_DIR}/${retired_private_file}")
        message(FATAL_ERROR
            "Retired private aggregation file was recreated: ${retired_private_file}")
    endif()
endforeach()

# Source implementation namespaces are separate from the installed SDK
# namespace manifest, but they still use one spelling. This prevents the
# retired Internal/impl layers from reappearing under a new private header.
file(GLOB_RECURSE aero_source_namespace_scan
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inl"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.inl")
set(source_namespace_forbidden_patterns
    "namespace[ \\t]+Internal([ \\t:{]|$)"
    "namespace[ \\t]+Aero::Internal([ \\t:{]|$)"
    "Aero::Internal::"
    "::Internal::"
    "namespace[ \\t]+impl([ \\t:{]|$)"
    "namespace[ \\t]+Aero::impl([ \\t:{]|$)"
    "::impl::"
    "namespace[ \\t]+Detail[ \\t]*\\{")
set(source_namespace_forbidden_matches)
foreach(pattern IN LISTS source_namespace_forbidden_patterns)
    aero_collect_matches(matches "${pattern}" ${aero_source_namespace_scan})
    list(APPEND source_namespace_forbidden_matches ${matches})
endforeach()
if(source_namespace_forbidden_matches)
    list(REMOVE_DUPLICATES source_namespace_forbidden_matches)
    message(FATAL_ERROR
        "Source private namespaces must use Aero::<Domain>::Detail; forbidden namespace spelling remains: "
        "${source_namespace_forbidden_matches}")
endif()
unset(aero_source_namespace_scan)
unset(source_namespace_forbidden_patterns)
unset(source_namespace_forbidden_matches)

# A translation unit has exactly one build owner. The D3D11 backend appears a
# second time only as an OBJECT_DEPENDS anchor for its implementation fragments;
# every other repeated source token is a duplicate compilation identity.
set(aero_source_manifest_files
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/cmake/AeroGuiTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroRuntimeTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroRenderingTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroProductTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroToolsTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroConformanceTargets.cmake")
set(aero_source_claim_keys)
foreach(manifest IN LISTS aero_source_manifest_files)
    file(READ "${manifest}" manifest_content)
    string(REGEX MATCHALL
        "src/[A-Za-z0-9_./-]+[.]cpp"
        manifest_source_claims "${manifest_content}")
    foreach(source IN LISTS manifest_source_claims)
        string(SHA256 source_key "${source}")
        if(NOT DEFINED aero_source_claim_${source_key})
            set(aero_source_claim_${source_key} 0)
            set(aero_source_path_${source_key} "${source}")
            list(APPEND aero_source_claim_keys "${source_key}")
        endif()
        math(EXPR aero_source_claim_${source_key}
            "${aero_source_claim_${source_key}} + 1")
    endforeach()
endforeach()
set(aero_duplicate_source_claims)
foreach(source_key IN LISTS aero_source_claim_keys)
    set(allowed_claims 1)
    if(aero_source_path_${source_key} STREQUAL
            "src/render/d3d11/D3D11Backend.cpp")
        set(allowed_claims 2)
    endif()
    if(aero_source_claim_${source_key} GREATER allowed_claims)
        list(APPEND aero_duplicate_source_claims
            "${aero_source_path_${source_key}} (${aero_source_claim_${source_key}} claims)")
    endif()
endforeach()
if(aero_duplicate_source_claims)
    message(FATAL_ERROR
        "Aero source files must have one compile owner: "
        "${aero_duplicate_source_claims}")
endif()
unset(aero_source_manifest_files)
unset(aero_source_claim_keys)
unset(aero_duplicate_source_claims)

file(GLOB_RECURSE aero_retired_private_reference_scan
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inl"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.inl"
    "${AERO_SOURCE_DIR}/docs/*.md"
    "${AERO_SOURCE_DIR}/docs/*.txt")
aero_collect_matches(retired_private_reference_matches
    "(AnimationInternal|BindingInternal|ElementInternal|InputInternal|LayoutInternal|MetadataInternal|PropertyInternal|RoutedEventInternal|StyleInternal|ControlInternals|ItemsInternal|TemplateInternals|MarkupInternal|MarkupWriterInternal|AnimationInternals|BrushInternals|EffectInternals|TransformInternals|DrawingInternals|RenderDeviceInternal)[.]hpp"
    ${aero_retired_private_reference_scan})
if(retired_private_reference_matches)
    list(REMOVE_DUPLICATES retired_private_reference_matches)
    message(FATAL_ERROR
        "Retired source-private header is still referenced: "
        "${retired_private_reference_matches}")
endif()
unset(aero_retired_private_reference_scan)
unset(retired_private_reference_matches)

file(GLOB_RECURSE input_runtime_files
    "${AERO_SOURCE_DIR}/src/gui/*.cpp"
    "${AERO_SOURCE_DIR}/src/gui/*.hpp")
aero_collect_matches(retired_input_managers
    "(CommandManager|HitTestManager|PointerInputManager|FocusManager|KeyboardInputManager|TextInputManager)"
    ${input_runtime_files})
if(retired_input_managers)
    message(FATAL_ERROR
        "Input internals must use the single InputRouter and private state types: "
        "${retired_input_managers}")
endif()

aero_collect_matches(split_ui_element_services
    "(eventRouter_|commandRouter_|(^|[^A-Za-z0-9_])manager_|handlerState_)"
    "${AERO_SOURCE_DIR}/include/Aero/UIElement.hpp")
if(split_ui_element_services)
    message(FATAL_ERROR
        "UIElement must use one View service attachment and semantic private state: "
        "${split_ui_element_services}")
endif()

aero_collect_matches(command_route_bypass
    "EventRoute[ \t]+[A-Za-z_]|[.]Build\\([A-Za-z_]+,[ \t]*RoutingStrategy"
    "${AERO_SOURCE_DIR}/src/gui/Commands.cpp")
if(command_route_bypass)
    message(FATAL_ERROR
        "Commands must traverse routes through EventRouter: ${command_route_bypass}")
endif()

file(GLOB_RECURSE render_runtime_files
    "${AERO_SOURCE_DIR}/src/render/*.cpp"
    "${AERO_SOURCE_DIR}/src/render/*.hpp"
    "${AERO_SOURCE_DIR}/src/integration/*.cpp"
    "${AERO_SOURCE_DIR}/src/integration/*.hpp"
    "${AERO_SOURCE_DIR}/src/runtime/*.cpp"
    "${AERO_SOURCE_DIR}/src/runtime/*.hpp")
aero_collect_matches(view_owned_document_implementation
    "XamlDocumentPrivate::Adopt|struct[ \t]+XamlDocument::Impl"
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp"
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp")
if(view_owned_document_implementation)
    message(FATAL_ERROR
        "XamlDocument implementation belongs to Markup, not View runtime: "
        "${view_owned_document_implementation}")
endif()

aero_collect_matches(view_owned_control_state_mutation
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp")
if(view_owned_control_state_mutation)
    message(FATAL_ERROR
        "Control private state mutation belongs to Controls: "
        "${view_owned_control_state_mutation}")
endif()

aero_collect_matches(ungated_window_surface_backends
    "#if[ \t]+defined[(]_WIN32[)]([ \t\r\n]*)$|#elif[ \t]+defined[(]__linux__[)]([ \t\r\n]*)$"
    "${AERO_SOURCE_DIR}/src/integration/OpenGL33Device.cpp")
if(ungated_window_surface_backends)
    message(FATAL_ERROR
        "OpenGL window endpoint backends must be gated by enabled surface options: "
        "${ungated_window_surface_backends}")
endif()

aero_collect_matches(retired_render_submission_layers
    "(RenderBackend|EndpointSubmissionBackend|QueryInternalService|TextBackendServiceId|MeshBackendServiceId|ImageBackendServiceId)"
    ${render_runtime_files})
if(retired_render_submission_layers)
    message(FATAL_ERROR
        "Retired render submission layers or service locators remain: "
        "${retired_render_submission_layers}")
endif()

aero_collect_matches(hidden_render_worker
    "(RenderSubmissionMode|DedicatedThread|WorkerMain|StartWorker|StopWorker|pendingFrameCount|coalescedFrameCount|highWatermark)"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/RenderDevice.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/D3D11.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/OpenGL33.hpp"
    "${AERO_SOURCE_DIR}/src/integration/RenderDevice.cpp"
    "${AERO_SOURCE_DIR}/src/integration/IntegrationPrivate.hpp"
    "${AERO_SOURCE_DIR}/src/integration/D3D11Device.cpp"
    "${AERO_SOURCE_DIR}/src/integration/OpenGL33Device.cpp"
    "${AERO_SOURCE_DIR}/src/integration/OpenGL33Device.cpp")
if(hidden_render_worker)
    message(FATAL_ERROR
        "Render scheduling belongs to the host; hidden endpoint workers or "
        "queues remain: ${hidden_render_worker}")
endif()

aero_collect_matches(hidden_endpoint_thread
    "(std::thread|condition_variable)"
    "${AERO_SOURCE_DIR}/src/integration/RenderDevice.cpp")
if(hidden_endpoint_thread)
    message(FATAL_ERROR
        "RenderDevice must not create or coordinate a private thread: "
        "${hidden_endpoint_thread}")
endif()

aero_collect_matches(render_tree_submission_leak
    "RenderDevice|Submit[ \t]*\\("
    "${AERO_SOURCE_DIR}/src/render/RenderTree.hpp")
if(render_tree_submission_leak)
    message(FATAL_ERROR
        "RenderTree must build immutable frames without owning submission: "
        "${render_tree_submission_leak}")
endif()

# J-series flat GUI kernel and tree-model gates.
file(GLOB gui_kernel_children LIST_DIRECTORIES true
    "${AERO_SOURCE_DIR}/src/gui/*")
set(gui_kernel_subdirectories)
foreach(gui_kernel_child IN LISTS gui_kernel_children)
    if(IS_DIRECTORY "${gui_kernel_child}")
        file(RELATIVE_PATH gui_kernel_relative
            "${AERO_SOURCE_DIR}" "${gui_kernel_child}")
        if(NOT gui_kernel_relative STREQUAL "src/gui/private")
            list(APPEND gui_kernel_subdirectories "${gui_kernel_relative}")
        endif()
    endif()
endforeach()
if(gui_kernel_subdirectories)
    message(FATAL_ERROR
        "src/gui is a flat WPF semantic kernel; subdirectories are not allowed: "
        "${gui_kernel_subdirectories}")
endif()

foreach(gui_internal_header IN ITEMS
        "src/gui/GuiPrivate.hpp")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${gui_internal_header}")
        message(FATAL_ERROR
            "Expected GUI domain header is missing: ${gui_internal_header}")
    endif()
endforeach()

file(GLOB_RECURSE tree_runtime_sources
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
aero_collect_matches(retired_tree_layers
    "(class[ \t]+ObjectTree|class[ \t]+MountService|class[ \t]+VisualTreeMount|ObjectTree::|MountService::|VisualTreeMount::)"
    ${tree_runtime_sources})
if(retired_tree_layers)
    message(FATAL_ERROR
        "Retired object-tree or mount layer was recreated: ${retired_tree_layers}")
endif()

aero_collect_matches(view_state_service_locator
    "Get(Metadata|EffectiveValues|LayoutEngine|RenderTree|BindingEngine|EventRouter|InputRouter|TemplateEngine|StyleEngine)[ \t]*\\("
    "${AERO_SOURCE_DIR}/include/Aero/View.hpp")
if(view_state_service_locator)
    message(FATAL_ERROR
        "View must not expose its internal engine graph: "
        "${view_state_service_locator}")
endif()

aero_collect_matches(retired_platform_target
    "(^|[^A-Za-z0-9_])AeroPlatform([^A-Za-z0-9_]|$)|Aero::_DetailPlatform"
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/cmake/AeroGuiTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroRuntimeTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroProductTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroInstall.cmake")
if(retired_platform_target)
    message(FATAL_ERROR
        "The forwarding AeroPlatform target was recreated: "
        "${retired_platform_target}")
endif()

aero_collect_matches(retired_gui_target_name
    "(^|[^A-Za-z0-9_])AeroCore([^A-Za-z0-9_]|$)|Aero::_Detail(Core|GuiKernel)"
    "${AERO_SOURCE_DIR}/cmake/AeroGuiTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroRenderingTargets.cmake"
    "${AERO_SOURCE_DIR}/cmake/AeroInstall.cmake")
if(retired_gui_target_name)
    message(FATAL_ERROR
        "Retired Core or _Detail GUI target naming remains: "
        "${retired_gui_target_name}")
endif()

# K-series final public-surface convergence gates.
foreach(required_public_entry IN ITEMS
        "include/Aero/View.hpp"
        "include/Aero/Markup.hpp"
        "include/Aero/Meta.hpp"
        "include/Aero/Value.hpp"
        "include/Aero/Integration/Platform.hpp")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${required_public_entry}")
        message(FATAL_ERROR
            "Required converged SDK entry is missing: ${required_public_entry}")
    endif()
endforeach()

file(READ "${AERO_SOURCE_DIR}/include/Aero/View.hpp" aero_view_header)
string(FIND "${aero_view_header}" "class AERO_API View" aero_view_begin)
if(aero_view_begin EQUAL -1)
    message(FATAL_ERROR "Unable to inspect the public View surface")
endif()
string(SUBSTRING "${aero_view_header}" ${aero_view_begin} -1
    aero_view_class_tail)
string(FIND "${aero_view_class_tail}" "\nprivate:" aero_view_private)
if(aero_view_private EQUAL -1)
    message(FATAL_ERROR "Unable to inspect the public View surface")
endif()
string(SUBSTRING "${aero_view_class_tail}" 0 ${aero_view_private}
    aero_view_public_surface)
if(aero_view_public_surface MATCHES
        "(Load[ \\t]*\\(|Parse[ \\t]*\\(|LoadCompiled[ \\t]*\\(|RunFrame|Advance(Time|AnimationTime)|FindNamedObject|NamedObjectCount)")
    message(FATAL_ERROR
        "View public API recreated loader, scheduler or namescope services")
endif()
unset(aero_view_header)
unset(aero_view_begin)
unset(aero_view_private)
unset(aero_view_class_tail)
unset(aero_view_public_surface)

file(READ "${AERO_SOURCE_DIR}/include/Aero/FrameworkElement.hpp"
    aero_framework_element_header)
string(FIND "${aero_framework_element_header}"
    "class AERO_API FrameworkElement" aero_framework_element_begin)
if(aero_framework_element_begin EQUAL -1)
    message(FATAL_ERROR
        "Unable to inspect the public FrameworkElement surface")
endif()
string(SUBSTRING "${aero_framework_element_header}"
    ${aero_framework_element_begin} -1 aero_framework_element_class_tail)
string(FIND "${aero_framework_element_class_tail}" "\nprivate:"
    aero_framework_element_private)
if(aero_framework_element_private EQUAL -1)
    message(FATAL_ERROR
        "Unable to inspect the public FrameworkElement surface")
endif()
string(SUBSTRING "${aero_framework_element_class_tail}" 0
    ${aero_framework_element_private} aero_framework_element_public_surface)
if(aero_framework_element_public_surface MATCHES
        "(GetRenderParent|GetRenderChildren|SetTemplatedParent|AuthoredTriggers|IsRenderValid|RenderRevision|NodeId[ \\t]*\\(|InvalidateRender[ \\t]*\\()")
    message(FATAL_ERROR
        "FrameworkElement public API exposes template or render runtime state")
endif()
unset(aero_framework_element_header)
unset(aero_framework_element_begin)
unset(aero_framework_element_private)
unset(aero_framework_element_class_tail)
unset(aero_framework_element_public_surface)

file(GLOB_RECURSE frame_pipeline_sources
    "${AERO_SOURCE_DIR}/src/runtime/*.cpp"
    "${AERO_SOURCE_DIR}/src/runtime/*.hpp"
    "${AERO_SOURCE_DIR}/src/render/*.cpp"
    "${AERO_SOURCE_DIR}/src/render/*.hpp"
    "${AERO_SOURCE_DIR}/src/integration/*.cpp"
    "${AERO_SOURCE_DIR}/src/integration/*.hpp")
aero_collect_matches(synchronous_frame_logging
    "(fprintf[ \\t]*\\([ \\t]*stderr|std::cerr|std::clog)"
    ${frame_pipeline_sources})
if(synchronous_frame_logging)
    message(FATAL_ERROR
        "Frame/runtime hot paths contain synchronous diagnostic I/O: "
        "${synchronous_frame_logging}")
endif()


# Stable View objects use one packed allocation. Reintroducing one allocation
# per engine increases startup cost, fragmentation and rollback complexity.
file(READ "${AERO_SOURCE_DIR}/src/runtime/View.cpp"
    aero_view_state_source)
foreach(required_arena_marker IN ITEMS
        "class ViewArena"
        "ViewArenaCapacity"
        "arena.Initialize"
        "arena.Create"
        "arena.Reset")
    string(FIND "${aero_view_state_source}"
        "${required_arena_marker}" required_arena_marker_position)
    if(required_arena_marker_position EQUAL -1)
        message(FATAL_ERROR
            "Packed per-View object allocation is incomplete: "
            "${required_arena_marker}")
    endif()
endforeach()
unset(aero_view_state_source)
unset(required_arena_marker)
unset(required_arena_marker_position)


file(READ "${AERO_SOURCE_DIR}/cmake/AeroAddXaml.cmake" aero_add_xaml_content)
if(aero_add_xaml_content MATCHES
        "(_Detail|runtime/|markup/GuiSchema|ModuleSet|aero_add_schema_manifest)")
    message(FATAL_ERROR
        "Installed AeroAddXaml.cmake leaks private implementation details")
endif()

# Final SDK names are canonical. Do not recreate transitional public aliases or
# expose frame diagnostics as part of the normal View authoring surface.
file(GLOB_RECURSE final_sdk_sources
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/tools/sdk-consumers/*.cpp")
aero_collect_matches(retired_final_sdk_names
    "(class[ \t]+AERO_API[ \t]+GUI|Aero::GUI|ViewFrameResult|class[ \t]+AERO_API[ \t]+RenderEndpoint)"
    ${final_sdk_sources})
if(retired_final_sdk_names)
    message(FATAL_ERROR
        "Transitional SDK names were recreated: ${retired_final_sdk_names}")
endif()

file(READ "${AERO_SOURCE_DIR}/include/Aero/Meta.hpp"
    aero_meta_header)
string(FIND "${aero_meta_header}"
    "TypeDescription<T> Register(" aero_meta_register_declaration)
if(aero_meta_register_declaration EQUAL -1)
    message(FATAL_ERROR
        "Meta::Registration/Register is not the canonical public authoring surface")
endif()

string(CONCAT retired_source_provider "Source" "Provider")
aero_collect_matches(retired_provider_api
    "(Aero::Providers|Aero/Integration/${retired_source_provider}|I${retired_source_provider}|${retired_source_provider}Adapter|Add${retired_source_provider}|Register${retired_source_provider})"
    ${final_sdk_sources} ${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake)
if(retired_provider_api)
    message(FATAL_ERROR
        "Retired provider names or paths remain in the SDK surface: ${retired_provider_api}")
endif()

# R4 SDK freeze gates. The installed object model must not regain per-object
# View/Render/Layout/Template service pointers or private XAML author proxies.
aero_collect_matches(public_runtime_service_fields
    "(renderRuntime_|layoutManager_|layoutService_|viewServices_|templateRuntime_|visualStateRuntime_|interactionRuntime_|interactions_|meshServices_)"
    ${aero_namespace_headers})
if(public_runtime_service_fields)
    message(FATAL_ERROR
        "Installed headers expose View/Render/Layout/Template runtime services: "
        "${public_runtime_service_fields}")
endif()

file(READ "${AERO_SOURCE_DIR}/include/Aero/Application.hpp"
    aero_application_header)
file(READ "${AERO_SOURCE_DIR}/include/Aero/Window.hpp"
    aero_window_header)
file(READ "${AERO_SOURCE_DIR}/src/app/Application.cpp"
    aero_application_source)
foreach(required_application_surface IN ITEMS
        "ResourceDictionary& GetResources() noexcept"
        "void SetMainWindow(Base::Ref<Window> value) noexcept"
        "void SetMainWindowBorrowed(Window* value) noexcept"
        "Base::Result<int> RunChecked() noexcept")
    string(FIND "${aero_application_header}"
        "${required_application_surface}" application_surface_index)
    if(application_surface_index EQUAL -1)
        message(FATAL_ERROR
            "R4 Application SDK surface regressed: ${required_application_surface}")
    endif()
endforeach()
foreach(required_window_surface IN ITEMS
        "void Show() noexcept"
        "Base::Result<void> ShowChecked() noexcept"
        "Base::Result<void> CloseChecked() noexcept")
    string(FIND "${aero_window_header}"
        "${required_window_surface}" window_surface_index)
    if(window_surface_index EQUAL -1)
        message(FATAL_ERROR
            "R4 Window SDK surface regressed: ${required_window_surface}")
    endif()
endforeach()
if(aero_application_source MATCHES "thread_local[ \t]+Application")
    message(FATAL_ERROR
        "Application::Current must be process-wide rather than thread_local")
endif()

file(GLOB_RECURSE aero_object_model_sources
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_matches(private_xaml_author_proxies
    "XamlVisual(State|Transition|States|StateGroup|StateManager)"
    ${aero_object_model_sources})
if(private_xaml_author_proxies)
    message(FATAL_ERROR
        "Private XAML author proxies recreated a second public object model: "
        "${private_xaml_author_proxies}")
endif()

set(aero_product_entry_checks
    "include/Aero/Gui.hpp|Aero::Gui"
    "include/Aero/App.hpp|Aero::App"
    "include/Aero/Integration.hpp|Aero::Integration"
    "include/Aero/Meta.hpp|Aero::Meta")
foreach(product_check IN LISTS aero_product_entry_checks)
    string(REPLACE "|" ";" product_parts "${product_check}")
    list(GET product_parts 0 product_header)
    list(GET product_parts 1 product_target)
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${product_header}")
        message(FATAL_ERROR "R4 product entry is missing: ${product_header}")
    endif()
    file(GLOB aero_product_target_files
        "${AERO_SOURCE_DIR}/CMakeLists.txt"
        "${AERO_SOURCE_DIR}/cmake/*.cmake")
    aero_collect_matches(product_target_match
        "add_library[ \t\r\n]*[(][ \t\r\n]*${product_target}[ \t]+ALIAS"
        ${aero_product_target_files})
    if(NOT product_target_match)
        message(FATAL_ERROR "R4 product target is missing: ${product_target}")
    endif()
endforeach()

file(READ "${AERO_SOURCE_DIR}/src/markup/MarkupLoader.cpp"
    aero_compiled_xaml_source)
foreach(axb2_marker IN ITEMS
        "0x32425841"
        "AxbSectionKind::Types"
        "AxbSectionKind::Members"
        "AxbSectionKind::Values"
        "AxbSectionKind::Instructions")
    string(FIND "${aero_compiled_xaml_source}"
        "${axb2_marker}" axb2_marker_position)
    if(axb2_marker_position EQUAL -1)
        message(FATAL_ERROR "AXB2 format marker is missing: ${axb2_marker}")
    endif()
endforeach()
file(READ "${AERO_SOURCE_DIR}/cmake/AeroAddXaml.cmake"
    aero_add_xaml_r4_content)
if(NOT aero_add_xaml_r4_content MATCHES "[.]axb\"")
    message(FATAL_ERROR "AeroAddXaml must emit AXB2 .axb artifacts")
endif()

file(READ "${AERO_SOURCE_DIR}/cmake/AeroInstall.cmake"
    aero_install_r4_content)
if(aero_install_r4_content MATCHES
        "list[ \t\r\n]*[(]APPEND[ \t]+_aero_sdk_targets[^)]*Objects")
    message(FATAL_ERROR
        "Installed CMake targets expose an internal object library")
endif()

message(STATUS "Aero architecture dependency checks passed")

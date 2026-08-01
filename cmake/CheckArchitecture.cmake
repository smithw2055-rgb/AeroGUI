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

if(EXISTS "${AERO_SOURCE_DIR}/include/Aero/Core")
    message(FATAL_ERROR
        "The installed SDK must not expose the retired include/Aero/Core tree")
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
    "${AERO_SOURCE_DIR}/src/gui/*.cpp"
    "${AERO_SOURCE_DIR}/src/gui/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Meta/*.hpp")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/src/diagnostics/Diagnostics.cpp"
    "${AERO_SOURCE_DIR}/src/gui/Dispatcher.cpp"
    "${AERO_SOURCE_DIR}/src/gui/ObjectFactory.cpp"
    "${AERO_SOURCE_DIR}/src/gui/PropertyInternal.hpp")
list(APPEND core_files
    "${AERO_SOURCE_DIR}/include/Aero/DependencyProperty.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Diagnostics.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Diagnostics/PropertyValueSource.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/RoutedEvent.hpp"
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
    "include/Aero/Core/Property/PropertyProviderSession.hpp"
    "include/Aero/Detail/UiMetadata.hpp"
    "include/Aero/RuntimeHost.hpp"
    "include/Aero/XamlReloadCoordinator.hpp"
    "include/Aero/BuiltinModules.hpp"
    "include/Aero/GuiSchema.hpp"
    "include/Aero/Markup/Loader.hpp"
    "include/Aero/Markup/Extensions.hpp"
    "include/Aero/Core/Events/RoutedEventTable.hpp"
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
    "include/Aero/ElementTree.hpp"
    "include/Aero/Rendering.hpp"
    "include/Aero/Data/Binding.hpp"
    "include/Aero/Documents/Documents.hpp"
    "include/Aero/Input/Commands.hpp"
    "include/Aero/Input/Navigation.hpp"
    "include/Aero/Media/Animation.hpp"
    "include/Aero/Core/Metadata/BindingPath.hpp"
    "include/Aero/Core/Metadata/BuiltinTypeIds.hpp"
    "include/Aero/Core/Metadata/CoreMetadata.hpp"
    "include/Aero/Core/Metadata/BehaviorTable.hpp"
    "include/Aero/Core/Metadata/MetadataValuePath.hpp"
    "include/Aero/Core/Metadata/ValueTable.hpp"
    "include/Aero/Core/ObjectFactoryState.hpp"
    "include/Aero/Core/Property/EffectiveValueEngine.hpp"
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

set(legacy_header_pattern
    "#[ \t]*include[ \t]*<Aero/Core/(Activation|BuiltinTypeIds|DependencyProperty|EffectiveValueEngine|BehaviorTable|MetadataDescriptors|MetaRegistry|MetadataDsl|MetadataId|RegistrationValues|MetaRegistry|MetadataValueFacets|MetadataValuePath|ValueTable|TypeRegistry|Value|Binding|Input|Layout|ElementTree|Rendering|Style|UI|RuntimeMetadata|ControlPrimitives|Controls)\\.hpp>")
file(GLOB_RECURSE current_code
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.inc"
    "${AERO_SOURCE_DIR}/tests/*.cpp"
    "${AERO_SOURCE_DIR}/tests/*.inc"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
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
    "${AERO_SOURCE_DIR}/include/Aero/View.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/ViewOptions.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/RenderDevice.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Integration/SourceProvider.hpp")
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


set(wpf_single_track_headers
    "${AERO_SOURCE_DIR}/include/Aero/UIElement.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/FrameworkElement.hpp")
aero_collect_matches(wpf_legacy_accessors
    "[ \t](DesiredSize|RenderSize|LayoutSlot|LayoutClip|IsMeasureValid|IsArrangeValid|ClipToBounds|IsHitTestVisible|IsVisible|IsEnabled|AllowDrop|IsMouseOver|IsPressed|IsKeyboardFocused|IsKeyboardFocusWithin|Focusable|IsTabStop|TabIndex|IsFocusScope|RenderTransform|RenderTransformOrigin|UseLayoutRounding|Width|Height|MinSize|MaxSize|Margin|LayoutTransform|LocalVisualTransform|RenderParent|RenderChildren)[ \t]*[(]"
    ${wpf_single_track_headers})
if(wpf_legacy_accessors)
    message(FATAL_ERROR
        "WPF-facing element headers expose legacy non-Get accessors: "
        "${wpf_legacy_accessors}")
endif()

aero_collect_matches(wpf_legacy_setters
    "[ \t](SetEnabled|SetHitTestVisible|SetTabStop|SetFocusScope|SetLayoutRounding)[ \t]*[(]"
    ${wpf_single_track_headers}
    "${AERO_SOURCE_DIR}/include/Aero/FrameworkElement.hpp")
if(wpf_legacy_setters)
    message(FATAL_ERROR
        "WPF-facing element headers expose property setters without the canonical property name: "
        "${wpf_legacy_setters}")
endif()

set(wpf_shape_headers
    "${AERO_SOURCE_DIR}/include/Aero/Shapes.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Panels.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Primitives.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Media/Brushes.hpp")
aero_collect_matches(wpf_shape_legacy_getters
    "[ \t](Fill|FillBrush|Stroke|StrokeBrush|StrokeThickness|RadiusX|RadiusY|LastChildFill)[ \t]*[(][)]"
    ${wpf_shape_headers})
if(wpf_shape_legacy_getters)
    message(FATAL_ERROR
        "WPF-facing property getters must use Get<Property>(): "
        "${wpf_shape_legacy_getters}")
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
    "(RoutedEventTable|DescriptionBuilder|ITextBlockLayoutService|TextBlockLayoutServiceScope|TextBlockRenderService|D3D11TextBlockRenderService|IGlyphRunResourceRegistry|DisplayListBuilder|RenderCommand|RenderImageId|RenderMeshId|RenderGlyphRunId|ThemeStyleRegistry|PPAAOutProperty|PasswordLengthProperty|RuntimeManagersFwd|RoutedHandlerStorage|RoutedHandlerTraits|Aero/Detail/|BuildEditorDisplayList|RuntimeAnimation\\(|RuntimeFrame\\(|RuntimeEasing\\(|ItemContainerGeneratorImpl[ \t]*[*]|VisualStateManagerImpl[ \t]*[*])"
    ${default_sdk_headers})
if(removed_public_services)
    message(FATAL_ERROR
        "Removed manager, metadata detail, or text service leaks through public headers: "
        "${removed_public_services}")
endif()


set(routed_event_headers
    "${AERO_SOURCE_DIR}/include/Aero/RoutedEvent.hpp"
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
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Base.hpp")
aero_collect_matches(public_template_runtime_surface
    "(DefaultStyleKey|TemplateGeneration|(^|[ \t])TemplateChild[ \t]*[(]|(^|[ \t])SetTemplateChild[ \t]*[(]|(^|[ \t])IsTemplateApplied[ \t]*[(])"
    ${control_authoring_headers})
if(public_template_runtime_surface)
    message(FATAL_ERROR
        "Control authoring headers expose template runtime state: "
        "${public_template_runtime_surface}")
endif()

aero_collect_matches(legacy_ui_element_collection_surface
    "(std::uint32_t[ \t]+Count|UIElement[*][ \t]+At|bool[ \t]+Empty)[ \t]*[(]"
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
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Base.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Items.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Panels.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Primitives.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Standard.hpp"
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

file(READ "${AERO_SOURCE_DIR}/CMakeLists.txt" root_cmake_content)
string(REGEX MATCHALL "\n" root_cmake_newlines "${root_cmake_content}")
list(LENGTH root_cmake_newlines root_cmake_line_count)
math(EXPR root_cmake_line_count "${root_cmake_line_count} + 1")
if(root_cmake_line_count GREATER 600)
    message(FATAL_ERROR
        "Root CMakeLists.txt exceeded the 600-line product composition budget")
endif()

file(GLOB_RECURSE physical_public_headers
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h")
list(LENGTH physical_public_headers physical_public_header_count)
if(physical_public_header_count GREATER 96)
    message(FATAL_ERROR
        "Installed SDK header count exceeded the 96-file convergence budget")
endif()

file(GLOB_RECURSE private_access_headers
    "${AERO_SOURCE_DIR}/src/*Access.hpp"
    "${AERO_SOURCE_DIR}/src/*/*Access.hpp"
    "${AERO_SOURCE_DIR}/src/*/*/*Access.hpp")
list(LENGTH private_access_headers private_access_header_count)
if(private_access_header_count GREATER 10)
    message(FATAL_ERROR
        "Private Access header count exceeded the consolidated 10-file budget")
endif()


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
        "src/gui/RoutedEventInternal.hpp"
        "src/gui/InputInternal.hpp"
        "src/gui/LayoutInternal.hpp"
        "src/gui/BindingInternal.hpp"
        "src/gui/AnimationInternal.hpp"
        "src/gui/StyleInternal.hpp"
        "src/gui/ElementInternal.hpp"
        "src/gui/MetaInternals.hpp"
        "src/gui/PropertyInternal.hpp"
        "src/controls/TemplateProgram.hpp"
        "src/controls/TemplateInstance.hpp"
        "src/controls/TemplateInternals.hpp"
                "src/platform/win32/InputRouters.hpp"
        "src/platform/win32/Window.hpp"
        "src/platform/x11/Window.hpp")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${required_private_header}")
        message(FATAL_ERROR
            "Required converged private header is missing: ${required_private_header}")
    endif()
endforeach()

foreach(retired_private_file IN ITEMS
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
    "XamlDocumentPrivate::Adopt|struct[ \t]+UiDocument::Impl"
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp"
    "${AERO_SOURCE_DIR}/src/runtime/View.cpp")
if(view_owned_document_implementation)
    message(FATAL_ERROR
        "UiDocument implementation belongs to Markup, not View runtime: "
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
    "${AERO_SOURCE_DIR}/src/integration/RenderDeviceInternal.hpp"
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
        list(APPEND gui_kernel_subdirectories "${gui_kernel_relative}")
    endif()
endforeach()
if(gui_kernel_subdirectories)
    message(FATAL_ERROR
        "src/gui is a flat WPF semantic kernel; subdirectories are not allowed: "
        "${gui_kernel_subdirectories}")
endif()

file(GLOB gui_kernel_cpp "${AERO_SOURCE_DIR}/src/gui/*.cpp")
file(GLOB gui_kernel_headers "${AERO_SOURCE_DIR}/src/gui/*.hpp")
list(LENGTH gui_kernel_cpp gui_kernel_cpp_count)
list(LENGTH gui_kernel_headers gui_kernel_header_count)
if(gui_kernel_cpp_count GREATER 30)
    message(FATAL_ERROR
        "Flat GUI kernel exceeded the 30-translation-unit budget: "
        "${gui_kernel_cpp_count}")
endif()
if(gui_kernel_header_count GREATER 12)
    message(FATAL_ERROR
        "Flat GUI kernel exceeded the 12-private-header budget: "
        "${gui_kernel_header_count}")
endif()

foreach(gui_internal_header IN ITEMS
        "src/gui/AnimationInternal.hpp"
        "src/gui/BindingInternal.hpp"
        "src/gui/ElementInternal.hpp"
        "src/gui/InputInternal.hpp"
        "src/gui/LayoutInternal.hpp"
        "src/gui/MetaInternals.hpp"
        "src/gui/PropertyInternal.hpp"
        "src/gui/RoutedEventInternal.hpp"
        "src/gui/StyleInternal.hpp")
    file(READ "${AERO_SOURCE_DIR}/${gui_internal_header}" gui_internal_content)
    string(REGEX MATCHALL "\n" gui_internal_newlines "${gui_internal_content}")
    list(LENGTH gui_internal_newlines gui_internal_line_count)
    math(EXPR gui_internal_line_count "${gui_internal_line_count} + 1")
    if(gui_internal_line_count GREATER 750)
        message(FATAL_ERROR
            "Consolidated GUI domain header exceeded 750 lines: "
            "${gui_internal_header}")
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
        "include/Aero/Markup/XamlReader.hpp"
        "include/Aero/Integration/Platform.hpp")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${required_public_entry}")
        message(FATAL_ERROR
            "Required converged SDK entry is missing: ${required_public_entry}")
    endif()
endforeach()

file(READ "${AERO_SOURCE_DIR}/include/Aero/View.hpp" aero_view_header)
string(FIND "${aero_view_header}" "class AERO_API View final" aero_view_begin)
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
        "(Load[ \\t]*\\(|Parse[ \\t]*\\(|LoadCompiled[ \\t]*\\(|RegisterSourceProvider|RunFrame|Advance(Time|AnimationTime)|FindNamedObject|NamedObjectCount)")
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

file(GLOB_RECURSE framework_element_sources
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_matches(retired_render_invalidation_name
    "InvalidateRender" ${framework_element_sources})
if(retired_render_invalidation_name)
    message(FATAL_ERROR
        "Use WPF-style InvalidateVisual instead of InvalidateRender: "
        "${retired_render_invalidation_name}")
endif()

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

message(STATUS "Aero architecture dependency checks passed")

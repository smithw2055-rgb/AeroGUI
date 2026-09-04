# WPF/XAML class library. Product implementation sources compile directly into
# AeroGui; offline tools link the product targets instead of recompiling object
# wrappers.

set(_aero_vendored_expat_target "")
set(_aero_expat_target "")
if(AERO_WITH_EXPAT)
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        get_filename_component(_aero_sibling_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party" ABSOLUTE)
        if(EXISTS "${_aero_sibling_third_party}/expat/expat/CMakeLists.txt")
            set(AERO_THIRD_PARTY_ROOT "${_aero_sibling_third_party}")
        endif()
    endif()
    set(_aero_expat_source "${AERO_THIRD_PARTY_ROOT}/expat/expat")
    if(EXISTS "${_aero_expat_source}/CMakeLists.txt")
        set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
        set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${_aero_expat_source}"
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/expat" EXCLUDE_FROM_ALL)
        set_target_properties(expat PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(_aero_expat_target expat::expat)
        set(_aero_vendored_expat_target expat)
    else()
        find_package(EXPAT QUIET)
        if(TARGET EXPAT::EXPAT)
            set(_aero_expat_target EXPAT::EXPAT)
        else()
            message(FATAL_ERROR
                "AERO_WITH_EXPAT requires Expat or ${_aero_expat_source}")
        endif()
    endif()
endif()

set(_aero_gui_core_sources
    src/gui/core/Freezable.cpp
    src/gui/core/Dispatcher.cpp
    src/gui/core/RoutedEvents.cpp
    src/gui/core/ObjectFactory.cpp
    src/gui/core/ContentElement.cpp
    src/gui/core/ElementTree.cpp
    src/gui/core/Invariants.cpp
    src/gui/core/LayoutEngine.cpp
    src/gui/core/PropertySystem.cpp
    src/gui/core/TryCast.cpp
    src/gui/core/Visual.cpp
    src/gui/core/UIElement.cpp
    src/gui/core/FrameworkElement.cpp
    src/gui/core/FrameworkContentElement.cpp
    src/gui/core/DependencyObject.cpp)

set(_aero_gui_meta_sources
    src/gui/meta/Metadata.cpp
    src/gui/meta/TypeRegistry.inl
    src/gui/meta/MetadataAuthoring.inl
    src/gui/meta/BehaviorTable.inl
    src/gui/meta/MetaTable.inl
    src/gui/meta/Registry.inl
    src/gui/meta/EnumMetadata.cpp
    src/gui/meta/BuiltinMetadata.cpp
    src/gui/meta/Value.cpp
    src/gui/meta/Animation.inl
    src/gui/meta/Elements.inl
    src/gui/meta/Input.inl
    src/gui/meta/Media.inl
    src/gui/meta/Resources.inl
    src/gui/meta/Styling.inl
    src/gui/meta/Support.inl
    src/gui/meta/Module.cpp
    src/gui/meta/BuiltinModules.cpp)

set(_aero_gui_data_sources
    src/gui/data/BindingPath.cpp
    src/gui/data/BindingInternal.hpp
    src/gui/data/Binding.cpp
    src/gui/data/BindingEvaluation.cpp
    src/gui/data/BindingOperations.cpp
    src/gui/data/BindingExpression.cpp
    src/gui/data/NotifyPropertyChanged.cpp
    src/gui/data/CollectionView.cpp)

set(_aero_gui_styles_sources
    src/gui/styles/Resources.cpp
    src/gui/styles/Style.cpp)

set(_aero_gui_input_sources
    src/gui/input/Commands.cpp
    src/gui/input/Input.cpp
    src/gui/input/Cursor.cpp
    src/gui/input/Cursors.cpp
    src/gui/input/Mouse.cpp
    src/gui/input/Keyboard.cpp
    src/gui/input/DataObject.cpp
    src/gui/input/DragDrop.cpp
    src/gui/input/Clipboard.cpp
    src/gui/input/OverlayHost.cpp)

set(_aero_gui_triggers_sources
    src/gui/triggers/BaseTrigger.cpp
    src/gui/triggers/Trigger.cpp
    src/gui/triggers/DataTrigger.cpp
    src/gui/triggers/Condition.cpp
    src/gui/triggers/MultiTrigger.cpp
    src/gui/triggers/MultiDataTrigger.cpp
    src/gui/triggers/TriggerEngine.cpp)

set(_aero_gui_interactivity_sources
    src/gui/interactivity/Interactivity.cpp
    src/gui/interactivity/InteractivityEngine.cpp
    src/gui/interactivity/InteractivityEngine.Behaviors.cpp
    src/gui/interactivity/InteractivityEngine.Triggers.cpp
    src/gui/interactivity/InteractivityEngine.Style.cpp
    src/gui/interactivity/BlendBehaviors.cpp)

set(_aero_gui_media_sources
    src/gui/media/AnimationEngineInternal.hpp
    src/gui/media/AnimationEngine.cpp
    src/gui/media/AnimationEngine.Apply.cpp
    src/gui/media/Animation.cpp
    src/gui/media/Brushes.cpp
    src/gui/media/Effects.cpp
    src/gui/media/Geometry.cpp
    src/gui/media/GeometryFlatten.cpp
    src/gui/media/StreamGeometry.cpp
    src/gui/media/StrokeTessellate.cpp
    src/gui/media/Pen.cpp
    src/gui/media/ImageCache.cpp
    src/gui/media/Images.cpp
    src/gui/media/MediaElement.cpp
    src/gui/media/StbImageImplementation.cpp
    src/gui/media/Transforms.cpp
    src/gui/media/StoryboardHostInternal.hpp
    src/gui/media/StoryboardHost.cpp
    src/gui/media/StoryboardHost.Properties.cpp
    src/gui/media/StoryboardHost.Timelines.cpp
    src/gui/media/StoryboardHost.Actions.cpp
    src/gui/media/StoryboardHost.Completions.cpp
    src/gui/media/StoryboardHost.Events.cpp)

set(_aero_gui_controls_sources
    src/gui/controls/RichText.cpp
    src/gui/controls/Bars.cpp
    src/gui/controls/Buttons.cpp
    src/gui/controls/ContentControls.cpp
    src/gui/controls/ControlBehavior.cpp
    src/gui/controls/Controls.cpp
    src/gui/controls/Panels.cpp
    src/gui/controls/Images.cpp
    src/gui/controls/Items.cpp
    src/gui/controls/ItemContainerGenerator.cpp
    src/gui/controls/ListView.cpp
    src/gui/controls/Menus.cpp
    src/gui/controls/ControlsMetadata.cpp
    src/gui/controls/ScrollCommon.hpp
    src/gui/controls/ScrollContentPresenter.cpp
    src/gui/controls/ScrollViewer.cpp
    src/gui/controls/ScrollBar.cpp
    src/gui/controls/ScrollBehavior.cpp
    src/gui/controls/Selection.cpp
    src/gui/controls/TextBoxCommon.hpp
    src/gui/controls/TextBox.cpp
    src/gui/controls/PasswordBox.cpp
    src/gui/controls/TextBoxSelection.cpp
    src/gui/controls/TextBoxBehavior.cpp
    src/gui/controls/Trees.cpp
    src/gui/controls/Virtualization.cpp
    src/gui/controls/VisualStateManager.cpp)

set(_aero_gui_markup_sources
    src/gui/markup/MarkupCommon.hpp
    src/gui/markup/XamlObjectWriterInternal.hpp
    src/gui/markup/MarkupExtensionHost.hpp
    src/gui/markup/XamlObjectWriterInternal.cpp
    src/gui/markup/XamlObjectWriterBuilderCore.cpp
    src/gui/markup/XamlObjectWriterPropertyApply.cpp
    src/gui/markup/XamlObjectWriterMarkupEval.cpp
    src/gui/markup/XamlObjectWriterNameScope.cpp
    src/gui/markup/XamlObjectWriter.cpp
    src/gui/markup/XamlMarkupExtensions.cpp
    src/gui/markup/BindingExtension.inl
    src/gui/markup/DynamicResourceExtension.inl
    src/gui/markup/StaticResourceExtension.inl
    src/gui/markup/LocExtension.inl
    src/gui/markup/TemplateBindingExtension.inl
    src/gui/markup/TypeExtension.inl
    src/gui/markup/StaticExtension.inl
    src/gui/markup/XamlParser.cpp
    src/gui/markup/XamlCompiledSchema.cpp
    src/gui/markup/XamlSchemaMetadata.cpp
    src/gui/markup/XamlSchemaManifest.cpp
    src/gui/markup/XamlSchemaContext.cpp
    src/gui/markup/StyleSupport.cpp
    src/gui/markup/TemplateSupport.cpp
    src/gui/markup/TemplateCompiler.cpp
    src/gui/markup/XamlCompiledDocument.cpp
    src/gui/markup/XamlDocumentCache.cpp
    src/gui/markup/XamlObjectLoader.cpp
    src/gui/markup/XamlLoader.cpp
    src/gui/markup/GuiSchema.cpp
    src/gui/markup/ReloadCoordinator.cpp
    src/gui/markup/XamlProvider.cpp
    src/gui/markup/XamlReader.cpp)

set(_aero_gui_text_sources
    src/gui/text/EditableText.cpp
    src/gui/text/FontManager.cpp
    src/gui/text/GlyphAtlas.cpp
    src/gui/text/TextLayout.cpp
    src/gui/text/TextPipeline.cpp
    src/gui/text/TextTypes.cpp
    src/gui/text/freetype/FreeTypeAdapter.cpp
    src/gui/text/harfbuzz/HarfBuzzAdapter.cpp)

set(_aero_gui_diagnostics_sources
    src/gui/diagnostics/Diagnostics.cpp
    src/gui/diagnostics/Inspector.cpp
)

set(_aero_gui_templates_sources
    src/gui/templates/Templates.cpp)

set(_aero_gui_documents_sources
    src/gui/documents/Documents.cpp
    src/gui/documents/Adorners.cpp)

set(_aero_gui_shapes_sources
    src/gui/shapes/Path.cpp
    src/gui/shapes/Shapes.cpp)

set(_aero_gui_render_contract_sources
    src/render/DrawingContext.cpp
    src/render/RenderTree.cpp
    src/render/RenderDevice.cpp
    src/render/RenderTarget.cpp
    src/render/FrameEncoder.cpp
    src/render/TextRenderer.cpp)

set(_aero_gui_composition_sources
    src/gui/Gui.cpp
    src/gui/View.cpp
    src/gui/ViewFrame.cpp
    src/gui/ViewInput.cpp
    src/gui/ViewFocus.cpp
    src/gui/ViewRender.cpp
    src/gui/ViewRenderer.hpp
    src/gui/ViewRenderer.cpp
    src/gui/ViewState.hpp
    src/gui/ViewDocuments.cpp)

set(_aero_gui_sources
    ${_aero_gui_core_sources}
    ${_aero_gui_meta_sources}
    ${_aero_gui_data_sources}
    ${_aero_gui_styles_sources}
    ${_aero_gui_input_sources}
    ${_aero_gui_triggers_sources}
    ${_aero_gui_interactivity_sources}
    ${_aero_gui_controls_sources}
    ${_aero_gui_documents_sources}
    ${_aero_gui_shapes_sources}
    ${_aero_gui_templates_sources}
    ${_aero_gui_markup_sources}
    ${_aero_gui_media_sources}
    ${_aero_gui_text_sources}
    ${_aero_gui_diagnostics_sources}
    ${_aero_gui_render_contract_sources}
    ${_aero_gui_composition_sources})

add_library(AeroGui ${AERO_LIBRARY_TYPE} ${_aero_gui_sources})
add_library(Aero::Gui ALIAS AeroGui)
target_include_directories(AeroGui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # B4: Meta.hpp pulls TypeBuilder authoring detail from src/gui/meta.
        # BUILD uses src/; INSTALL uses the aero-meta-authoring prefix only.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/aero-meta-authoring>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroGui PUBLIC Aero::Base Threads::Threads)
if(AERO_WITH_EXPAT)
    if(_aero_vendored_expat_target)
        target_link_libraries(AeroGui PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:Aero::_PrivateExpat>)
    else()
        target_link_libraries(AeroGui PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:EXPAT::EXPAT>)
    endif()
endif()
target_compile_definitions(AeroGui PRIVATE
    AERO_GUI_IMPLEMENTATION=1
    $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_GUI_EXPORTS>
    AERO_UI_RESOURCE_MODEL=2
    AERO_CONTROLS_TEMPLATE_ABI=10
    AERO_MARKUP_UI_RESOURCES=1
    AERO_WITH_EXPAT=$<BOOL:${AERO_WITH_EXPAT}>)
target_compile_features(AeroGui PUBLIC cxx_std_17)
set_target_properties(AeroGui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
aero_apply_compiler_options(AeroGui)
aero_verify_windows_exports(AeroGui "Gui@Aero@@")

# Backend-neutral render contracts are implemented and exported by AeroGui,
# but have their own installed include prefix and CMake consumption boundary.
# AeroRender deliberately creates no additional DLL.
add_library(AeroRender INTERFACE)
add_library(Aero::Render ALIAS AeroRender)
target_link_libraries(AeroRender INTERFACE Aero::Gui)
target_compile_features(AeroRender INTERFACE cxx_std_17)

add_library(AeroGuiHeaderConsumer OBJECT tools/sdk-consumers/GuiConsumer.cpp)
target_link_libraries(AeroGuiHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroGuiHeaderConsumer)
add_library(AeroRenderHeaderConsumer OBJECT
    tools/sdk-consumers/RenderConsumer.cpp)
target_link_libraries(AeroRenderHeaderConsumer PRIVATE Aero::Render)
aero_apply_compiler_options(AeroRenderHeaderConsumer)
add_library(AeroEventsTriggersHeaderConsumer OBJECT
    tools/sdk-consumers/EventsTriggersConsumer.cpp)
target_link_libraries(AeroEventsTriggersHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroEventsTriggersHeaderConsumer)
add_library(AeroMetaHeaderConsumer OBJECT tools/sdk-consumers/MetaConsumer.cpp)
target_link_libraries(AeroMetaHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroMetaHeaderConsumer)

unset(_aero_gui_sources)
unset(_aero_gui_core_sources)
unset(_aero_gui_meta_sources)
unset(_aero_gui_data_sources)
unset(_aero_gui_styles_sources)
unset(_aero_gui_input_sources)
unset(_aero_gui_triggers_sources)
unset(_aero_gui_interactivity_sources)
unset(_aero_gui_controls_sources)
unset(_aero_gui_documents_sources)
unset(_aero_gui_shapes_sources)
unset(_aero_gui_templates_sources)
unset(_aero_gui_markup_sources)
unset(_aero_gui_media_sources)
unset(_aero_gui_text_sources)
unset(_aero_gui_diagnostics_sources)
unset(_aero_gui_render_contract_sources)
unset(_aero_gui_composition_sources)

function(aero_complete_gui_target)
# View/Gui composition is folded directly into AeroGui as a source group.
# It is not a separate object library or SDK binary.
#
# In-tree aero-xamlc/aero-schema-gen link Aero::Gui, so they can never be build
# prerequisites of AeroGui itself. Gui composition uses compiled built-in themes only
# when an independent host tool chain is supplied. Otherwise a source-fallback
# header is generated synchronously at configure time; the in-tree tools remain
# ordinary post-Gui tools and AeroCompiledThemes stays an explicit asset target.
set(_aero_gui_precompiled_themes OFF)
if(AERO_PRECOMPILE_BUILTIN_THEMES AND
   NOT "${AERO_HOST_XAMLC_EXECUTABLE}" STREQUAL "")
    if(CMAKE_CROSSCOMPILING OR
       NOT "${AERO_BUILTIN_SCHEMA_MANIFEST}" STREQUAL "" OR
       NOT "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}" STREQUAL "")
        set(_aero_gui_precompiled_themes ON)
    endif()
endif()

set(_aero_gui_theme_include_dir
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(NOT _aero_gui_precompiled_themes)
    # Gui fallback must not share an output with AeroCompiledThemes. Ninja
    # otherwise binds View.cpp's generated-header dependency to aero-xamlc and
    # recreates the AeroGui -> aero-xamlc -> AeroGui bootstrap cycle.
    set(_aero_gui_theme_include_dir
        "${CMAKE_CURRENT_BINARY_DIR}/gui-generated")
    set(_aero_gui_theme_header
        "${_aero_gui_theme_include_dir}/Aero/BuiltinThemes.generated.hpp")
    file(MAKE_DIRECTORY
        "${_aero_gui_theme_include_dir}/Aero")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_aero_gui_theme_header}"
            "-DLIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "-DDARK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "-DGENERIC_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            "-DEXT_LIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.LightBlue.xaml"
            "-DEXT_DARK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.DarkBlue.xaml"
            "-DEXT_LIGHT_BRUSHES_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Brushes.LightBlue.xaml"
            "-DEXT_DARK_BRUSHES_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Brushes.DarkBlue.xaml"
            "-DEXT_LIGHT_COLORS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Colors.Light.xaml"
            "-DEXT_DARK_COLORS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Colors.Dark.xaml"
            "-DEXT_FONTS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Fonts.xaml"
            "-DEXT_STYLES_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/AeroGUIExtensions/AeroTheme.Styles.xaml"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        RESULT_VARIABLE _aero_theme_embed_result)
    if(NOT _aero_theme_embed_result EQUAL 0)
        message(FATAL_ERROR
            "Unable to generate the built-in theme source fallback header")
    endif()
endif()

if(_aero_gui_precompiled_themes)
    target_sources(AeroGui PRIVATE "${_aero_generated_theme_header}")
    add_dependencies(AeroGui AeroCompiledThemes)
endif()
target_include_directories(AeroGui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${_aero_gui_theme_include_dir}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(AERO_DEFAULT_THEME_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml")
# TARGET_FILE_DIR creates the required AeroDefaultThemes -> AeroGui ordering.
# Do not add the reverse AeroGui -> AeroDefaultThemes dependency.
add_custom_target(AeroDefaultThemes ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${AERO_DEFAULT_THEME_FILES}
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    DEPENDS ${AERO_DEFAULT_THEME_FILES}
    VERBATIM)

unset(_aero_gui_precompiled_themes)
unset(_aero_gui_theme_include_dir)
unset(_aero_gui_theme_header)
unset(_aero_theme_embed_result)

# Private retained renderer and backend-neutral render-device machinery remain
# part of the explicit render-contract and composition lists above.

# Shared GPU StateCache (header-only) under src/render/common/ is included by
# both AeroRenderOpenGL33 and AeroRenderD3D11 via PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src.
# OpenGL 3.3 is a separately linkable backend product. AeroGui contains no GL
# implementation or factory symbols.
add_library(AeroRenderOpenGL33 ${AERO_LIBRARY_TYPE}
    src/render/opengl33/OpenGL33RenderDevice.cpp
            src/render/opengl33/OpenGL33Factories.cpp
    src/render/opengl33/OpenGL33RenderContext.cpp)
add_library(Aero::RenderOpenGL33 ALIAS AeroRenderOpenGL33)
target_include_directories(AeroRenderOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroRenderOpenGL33 PUBLIC Aero::Render)
target_compile_definitions(AeroRenderOpenGL33 PRIVATE
    AERO_GUI_IMPLEMENTATION=1
    $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_RENDER_OPENGL33_EXPORTS>)
target_compile_features(AeroRenderOpenGL33 PUBLIC cxx_std_17)
set_target_properties(AeroRenderOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
aero_apply_compiler_options(AeroRenderOpenGL33)
aero_verify_windows_exports(
    AeroRenderOpenGL33
    "CreateDevice@OpenGL33@Render@Aero@@"
    "CreateTarget@OpenGL33@Render@Aero@@"
    3)

if(AERO_ENABLE_WGL_SURFACE)
    target_sources(AeroRenderOpenGL33 PRIVATE
        src/render/platform/win32/OpenGLWindow.cpp)
endif()
if(AERO_ENABLE_GLX_SURFACE)
    target_sources(AeroRenderOpenGL33 PRIVATE
        src/render/platform/x11/OpenGLWindow.cpp)
endif()

if(AERO_ENABLE_WGL_SURFACE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_WGL_SURFACE is only supported on Windows")
    endif()
    target_link_libraries(AeroRenderOpenGL33 PRIVATE
        gdi32 opengl32 user32)
    target_compile_definitions(
        AeroRenderOpenGL33 PRIVATE AERO_HAS_WGL_SURFACE=1)
else()
    target_compile_definitions(
        AeroRenderOpenGL33 PRIVATE AERO_HAS_WGL_SURFACE=0)
endif()

if(AERO_ENABLE_GLX_SURFACE)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "AERO_ENABLE_GLX_SURFACE is only supported on Unix/X11")
    endif()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(AeroRenderOpenGL33 PRIVATE
        X11::X11 OpenGL::GL Threads::Threads)
    target_compile_definitions(
        AeroRenderOpenGL33 PRIVATE AERO_HAS_GLX_SURFACE=1)
else()
    target_compile_definitions(
        AeroRenderOpenGL33 PRIVATE AERO_HAS_GLX_SURFACE=0)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC diagnoses a reference obtained through a temporary Span view even
    # though the view points into the longer-lived immutable RenderFrame.
    target_compile_options(AeroGui PRIVATE -Wno-dangling-reference)
endif()

if(AERO_ENABLE_D3D11_BACKEND)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND is only supported on Windows")
    endif()

    set(_aero_d3d11_shader_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/d3d11-shaders")
    set(_aero_d3d11_render_frame_max_rectangle_instances 64)
    set(_aero_d3d11_fxc_hints
        "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    file(GLOB _aero_d3d11_fxc_sdk_directories
        LIST_DIRECTORIES true
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(APPEND _aero_d3d11_fxc_hints ${_aero_d3d11_fxc_sdk_directories})
    find_program(AERO_D3D11_FXC_EXECUTABLE
        NAMES fxc.exe
        HINTS ${_aero_d3d11_fxc_hints}
        DOC "Windows SDK fxc executable used to compile Aero D3D11 shaders")
    if(NOT AERO_D3D11_FXC_EXECUTABLE)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND requires the Windows SDK x64 fxc.exe")
    endif()

    # Keep shader declarations data-driven. Each pair follows the same naming
    # convention consumed by D3D11Shaders.cpp; only the source and instance-limit
    # requirement vary.
    set_property(GLOBAL PROPERTY AERO_D3D11_SHADER_OUTPUTS "")
    function(aero_compile_d3d11_shader_pair stem source use_instance_limit)
        foreach(stage IN ITEMS Vertex Pixel)
            if(stage STREQUAL "Vertex")
                set(profile vs_4_0)
                set(entry vs_main)
            else()
                set(profile ps_4_0)
                set(entry ps_main)
            endif()
            set(output
                "${_aero_d3d11_shader_directory}/AeroD3D11${stem}${stage}Shader.hpp")
            set(symbol "AeroD3D11${stem}${stage}Shader")
            set(defines)
            if(use_instance_limit)
                list(APPEND defines
                    /D "AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}")
            endif()
            add_custom_command(
                OUTPUT "${output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${_aero_d3d11_shader_directory}"
                COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
                    /T ${profile} /E ${entry}
                    ${defines}
                    /Vn ${symbol}
                    /Fh "${output}"
                    "${source}"
                DEPENDS "${source}"
                VERBATIM)
            set_property(GLOBAL APPEND PROPERTY
                AERO_D3D11_SHADER_OUTPUTS "${output}")
        endforeach()
    endfunction()

    set(_aero_d3d11_shader_root
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/shaders")
    aero_compile_d3d11_shader_pair(
        RenderFrameSolid "${_aero_d3d11_shader_root}/RenderFrameSolid.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFramePattern "${_aero_d3d11_shader_root}/RenderFramePattern.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameSDF "${_aero_d3d11_shader_root}/RenderFrameSDF.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameBlur "${_aero_d3d11_shader_root}/RenderFrameBlur.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameShadow "${_aero_d3d11_shader_root}/RenderFrameShadow.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameMask "${_aero_d3d11_shader_root}/RenderFrameMask.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameLinear "${_aero_d3d11_shader_root}/RenderFrameLinear.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameRadial "${_aero_d3d11_shader_root}/RenderFrameRadial.hlsl" FALSE)
    get_property(_aero_d3d11_shader_outputs GLOBAL PROPERTY
        AERO_D3D11_SHADER_OUTPUTS)
    add_custom_target(AeroD3D11RenderFrameShaders
        DEPENDS ${_aero_d3d11_shader_outputs})

    add_library(AeroRenderD3D11 ${AERO_LIBRARY_TYPE}
        src/render/d3d11/D3D11RenderDevice.cpp
                src/render/d3d11/D3D11Device.cpp
                src/render/d3d11/D3D11RenderContext.cpp)
    add_library(Aero::RenderD3D11 ALIAS AeroRenderD3D11)
    add_dependencies(AeroRenderD3D11 AeroD3D11RenderFrameShaders)
    target_include_directories(AeroRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
            "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroRenderD3D11
        PUBLIC Aero::Render
        PRIVATE d3d11 dxgi d3dcompiler)
    target_compile_definitions(AeroRenderD3D11 PRIVATE
        AERO_GUI_IMPLEMENTATION=1
        $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_RENDER_D3D11_EXPORTS>)
    target_compile_features(AeroRenderD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRenderD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES)
    aero_apply_compiler_options(AeroRenderD3D11)
aero_verify_windows_exports(
        AeroRenderD3D11
        "CreateDevice@D3D11@Render@Aero@@"
        "CreateTarget@D3D11@Render@Aero@@"
        3)

    unset(_aero_d3d11_shader_outputs)
    unset(_aero_d3d11_shader_root)
    unset(_aero_d3d11_backend_fragments)
endif()

# AeroGui owns the backend-neutral WPF/XAML object model, View runtime and
# providers. Native backend factories live only in the Render* products.
target_include_directories(AeroGui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroGui PRIVATE
    Aero::Audio freetype harfbuzz)

add_library(AeroGuiCompositionHeaderConsumer OBJECT
    tools/sdk-consumers/GuiCompositionConsumer.cpp)
target_link_libraries(
    AeroGuiCompositionHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroGuiCompositionHeaderConsumer)

add_library(AeroProvidersHeaderConsumer OBJECT
    tools/sdk-consumers/ProvidersConsumer.cpp)
target_link_libraries(
    AeroProvidersHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroProvidersHeaderConsumer)

if(TARGET AeroRenderD3D11)
    add_library(AeroD3D11HeaderConsumer OBJECT
        tools/sdk-consumers/D3D11Consumer.cpp)
    target_link_libraries(
        AeroD3D11HeaderConsumer PRIVATE Aero::RenderD3D11)
    aero_apply_compiler_options(AeroD3D11HeaderConsumer)

    add_executable(AeroD3D11LinkConsumer
        tools/sdk-consumers/BackendLinkConsumer.cpp)
    target_compile_definitions(
        AeroD3D11LinkConsumer PRIVATE AERO_LINK_D3D11=1)
    target_link_libraries(
        AeroD3D11LinkConsumer PRIVATE Aero::RenderD3D11)
    aero_apply_compiler_options(AeroD3D11LinkConsumer)
endif()

add_library(AeroOpenGL33HeaderConsumer OBJECT
    tools/sdk-consumers/OpenGL33Consumer.cpp)
target_link_libraries(
    AeroOpenGL33HeaderConsumer PRIVATE Aero::RenderOpenGL33)
aero_apply_compiler_options(AeroOpenGL33HeaderConsumer)

add_executable(AeroOpenGL33LinkConsumer
    tools/sdk-consumers/BackendLinkConsumer.cpp)
target_compile_definitions(
    AeroOpenGL33LinkConsumer PRIVATE AERO_LINK_OPENGL33=1)
target_link_libraries(
    AeroOpenGL33LinkConsumer PRIVATE Aero::RenderOpenGL33)
aero_apply_compiler_options(AeroOpenGL33LinkConsumer)

endfunction()

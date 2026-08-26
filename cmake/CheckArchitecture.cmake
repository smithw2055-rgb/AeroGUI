if(NOT DEFINED AERO_SOURCE_DIR OR AERO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()
if(POLICY CMP0007)
    cmake_policy(SET CMP0007 NEW)
endif()

# Final product invariants only. Migration spellings and temporary implementation
# layers belong in Git history, not in the permanent build contract.
function(aero_require_file relative_path)
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Required architecture file is missing: ${relative_path}")
    endif()
endfunction()

function(aero_forbid_file relative_path)
    if(EXISTS "${AERO_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Retired architecture file was recreated: ${relative_path}")
    endif()
endfunction()

function(aero_require_text relative_path needle description)
    file(READ "${AERO_SOURCE_DIR}/${relative_path}" content)
    string(FIND "${content}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${description}: ${relative_path}")
    endif()
endfunction()

function(aero_forbid_text relative_path needle description)
    file(READ "${AERO_SOURCE_DIR}/${relative_path}" content)
    string(FIND "${content}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${description}: ${relative_path}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Installed SDK surface
# ---------------------------------------------------------------------------
foreach(required_public_entry IN ITEMS
        "include/Aero/Gui.hpp"
        "include/AeroAudio/Audio.hpp"
        "include/AeroApp/Application.hpp"
        "include/AeroApp/Window.hpp"
        "include/AeroApp/WindowInterop.hpp"
        "include/Aero/InputInterop.hpp"
        "include/Aero/DependencyObject.hpp"
        "include/Aero/TryCast.hpp"
        "include/Aero/DependencyProperty.hpp"
        "include/Aero/RoutedEvent.hpp"
        "include/Aero/Visual.hpp"
        "include/Aero/UIElement.hpp"
        "include/Aero/FrameworkElement.hpp"
        "include/Aero/FrameworkContentElement.hpp"
        "include/Aero/Controls/Control.hpp"
        "include/Aero/Controls/ContentControl.hpp"
        "include/Aero/Controls/Panel.hpp"
        "include/Aero/Controls/ButtonBase.hpp"
        "include/Aero/Controls/Primitives/ButtonBase.hpp"
        "include/Aero/Controls/Primitives/ToggleButton.hpp"
        "include/Aero/Controls/Primitives/RepeatButton.hpp"
        "include/Aero/Controls/Button.hpp"
        "include/Aero/Controls/ToggleButton.hpp"
        "include/Aero/Controls/Grid.hpp"
        "include/Aero/Controls/StackPanel.hpp"
        "include/Aero/Controls/DockPanel.hpp"
        "include/Aero/Controls/WrapPanel.hpp"
        "include/Aero/Controls/UniformGrid.hpp"
        "include/Aero/Controls/Canvas.hpp"
        "include/Aero/Controls/Border.hpp"
        "include/Aero/Controls/Viewbox.hpp"
        "include/Aero/ContentElement.hpp"
        "include/Aero/VisualTreeHelper.hpp"
        "include/Aero/LogicalTreeHelper.hpp"
        "include/Aero/Controls/HeaderedContentControl.hpp"
        "include/Aero/Controls/GroupBox.hpp"
        "include/Aero/Controls/Label.hpp"
        "include/Aero/Controls/Expander.hpp"
        "include/Aero/Controls/TabItem.hpp"
        "include/Aero/Controls/TabControl.hpp"
        "include/Aero/Controls/TabPanel.hpp"
        "include/Aero/Controls/ItemsControl.hpp"
        "include/Aero/Controls/ListBox.hpp"
        "include/Aero/Controls/TreeView.hpp"
        "include/Aero/Controls/TextBlock.hpp"
        "include/Aero/Controls/TextBoxBase.hpp"
        "include/Aero/Controls/TextBox.hpp"
        "include/Aero/Controls/Primitives.hpp"
        "include/Aero/Controls/Primitives/Thumb.hpp"
        "include/Aero/Controls/Primitives/Track.hpp"
        "include/Aero/Controls/Primitives/RangeBase.hpp"
        "include/Aero/Controls/Primitives/ScrollBar.hpp"
        "include/Aero/Controls/Primitives/TickBar.hpp"
        "include/Aero/Controls/RangeBase.hpp"
        "include/Aero/Controls/Slider.hpp"
        "include/Aero/Controls/ProgressBar.hpp"
        "include/Aero/Controls/GridSplitter.hpp"
        "include/Aero/Data/Binding.hpp"
        "include/Aero/Resources.hpp"
        "include/Aero/Style.hpp"
        "include/Aero/FrameworkTemplate.hpp"
        "include/Aero/Controls/ControlTemplate.hpp"
        "include/Aero/VisualStateManager.hpp"
        "include/Aero/DataTemplate.hpp"
        "include/Aero/Media/Animation.hpp"
        "include/Aero/Media/Brushes.hpp"
        "include/Aero/Media/FontProvider.hpp"
        "include/Aero/Media/Fonts.hpp"
        "include/Aero/Media/Geometry.hpp"
        "include/Aero/Media/Transforms.hpp"
        "include/Aero/Markup/XamlReader.hpp"
        "include/Aero/Media/CompositionTarget.hpp"
        "include/Aero/View.hpp"
        "include/Aero/IRenderer.hpp"
        "include/Aero/Visibility.hpp"
        "include/Aero/HorizontalAlignment.hpp"
        "include/Aero/Controls/GridLength.hpp"
        "include/Aero/Media/BlendMode.hpp"
        "include/Aero/Diagnostics/Layout.hpp"
        "include/AeroRender/Render.hpp"
        "include/AeroRender/RenderDevice.hpp"
        "include/AeroRender/Texture.hpp"
        "include/AeroRender/RenderTarget.hpp"
        "include/AeroRender/D3D11.hpp"
        "include/AeroRender/OpenGL33.hpp"
        "include/Aero/Markup/ResourceScope.hpp"
        "include/Aero/Diagnostics/Rendering.hpp"
        "include/Aero/Meta.hpp"
        "include/AeroApp/App.hpp")
    aero_require_file("${required_public_entry}")
endforeach()

foreach(aero_root_value_alias IN ITEMS
        "include/Aero/Base/Ref.hpp|using Ref = Base::Ref<T>;"
        "include/Aero/Base/Result.hpp|using Result = Base::Result<T>;"
        "include/Aero/Base/String.hpp|using String = Base::String;"
        "include/Aero/Base/StringView.hpp|using StringView = Base::StringView;"
        "include/Aero/Base/Span.hpp|using Span = Base::Span<T>;"
        "include/Aero/Base/Object.hpp|using Object = Base::Object;")
    string(REPLACE "|" ";" aero_root_value_alias_parts
        "${aero_root_value_alias}")
    list(GET aero_root_value_alias_parts 0 aero_root_value_alias_header)
    list(GET aero_root_value_alias_parts 1 aero_root_value_alias_text)
    aero_require_text(
        "${aero_root_value_alias_header}"
        "${aero_root_value_alias_text}"
        "Ordinary SDK value spellings must be available directly under Aero")
endforeach()

foreach(aero_primary_sdk_header IN ITEMS
        "include/AeroApp/App.hpp"
        "include/AeroApp/Application.hpp"
        "include/AeroApp/Window.hpp"
        "include/Aero/Gui.hpp"
        "include/Aero/View.hpp"
        "include/Aero/IRenderer.hpp"
        "include/AeroAudio/Audio.hpp"
        "include/Aero/InputInterop.hpp"
        "include/Aero/Module.hpp"
        "include/AeroRender/Render.hpp"
        "include/AeroRender/RenderDevice.hpp"
        "include/AeroRender/RenderTarget.hpp"
        "include/AeroRender/D3D11.hpp"
        "include/AeroRender/OpenGL33.hpp")
    foreach(aero_low_level_spelling IN ITEMS
            "Base::Ref"
            "Base::Result"
            "Base::String"
            "Base::StringView"
            "Base::Span")
        aero_forbid_text(
            "${aero_primary_sdk_header}"
            "${aero_low_level_spelling}"
            "Primary SDK entry signatures must use the root Aero spelling")
    endforeach()
endforeach()

foreach(retired_public_entry IN ITEMS
        "include/Aero/Integration.hpp"
        "include/Aero/Integration"
        "include/Aero/RenderSurface.hpp"
        "include/Aero/Runtime.hpp"
        "include/Aero/RuntimeEnvironment.hpp"
        "include/Aero/RuntimeTypes.hpp"
        "include/Aero/RenderDevice.hpp"
        "include/Aero/RenderTarget.hpp"
        "include/Aero/Gui/RenderDevice.hpp"
        "include/Aero/Gui/RenderTarget.hpp"
        "include/Aero/App.hpp"
        "include/Aero/Application.hpp"
        "include/Aero/Window.hpp"
        "include/Aero/WindowInterop.hpp"
        "include/Aero/App/WindowInterop.hpp"
        "include/Aero/App"
        "include/Aero/Audio.hpp"
        "include/Aero/Audio/Audio.hpp"
        "include/Aero/Audio"
        "include/Aero/Render"
        "include/Aero/Input/Platform.hpp"
        "include/Aero/Input"
        "include/Aero/Platform"
        "include/Aero/Text/FontProvider.hpp"
        "include/Aero/Text"
        "include/Aero/Markup.hpp")
    aero_forbid_file("${retired_public_entry}")
endforeach()

aero_require_text(
    "include/Aero/IRenderer.hpp"
    "RenderTarget& target"
    "IRenderer must render to the canonical RenderTarget")
aero_forbid_text(
    "include/Aero/IRenderer.hpp"
    "RenderSurface"
    "IRenderer must not expose the retired RenderSurface spelling")
aero_require_text(
    "include/AeroRender/RenderTarget.hpp"
    "class AERO_GUI_API RenderTarget : public Base::Object"
    "RenderTarget must be the installed target object")
aero_forbid_text(
    "include/AeroRender/RenderTarget.hpp"
    "PresentMode"
    "Presentation policy must remain source-private")
aero_require_text(
    "include/Aero/Module.hpp"
    "Span<const Markup::ResourceScopeRegistration> resourceScopes;"
    "Product resource scopes must cross the Gui boundary through module data")
aero_forbid_text(
    "src/gui/markup/XamlLoader.cpp"
    "AeroApp/Application.hpp"
    "AeroGui Markup must not include the AeroApp Application contract")
aero_forbid_text(
    "src/gui/markup/XamlLoader.cpp"
    "Aero::Application"
    "AeroGui Markup must not link back to AeroApp symbols")
aero_require_text(
    "include/AeroApp/App.hpp"
    "AppMetadataModule()"
    "Custom App hosts must be able to register Application and Window metadata")
aero_require_text(
    "src/app/Metadata.cpp"
    "module.resourceScopes = resourceScopes;"
    "The App module must provide its Application resource-scope capability")

foreach(backend_header IN ITEMS
        "include/AeroRender/D3D11.hpp"
        "include/AeroRender/OpenGL33.hpp")
    aero_forbid_text(
        "${backend_header}"
        "WindowSurface"
        "Window presentation belongs to App/private backend code")
    aero_forbid_text(
        "${backend_header}"
        "NativeWindow"
        "Render backend SDK must not own native window hosting")
endforeach()
aero_require_text(
    "include/AeroRender/D3D11.hpp"
    "namespace Aero::Render::D3D11"
    "D3D11 backend declarations must use a dedicated namespace")
aero_require_text(
    "include/AeroRender/OpenGL33.hpp"
    "namespace Aero::Render::OpenGL33"
    "OpenGL backend declarations must use a dedicated namespace")
foreach(backend_header IN ITEMS
        "include/AeroRender/D3D11.hpp"
        "include/AeroRender/OpenGL33.hpp")
    aero_require_text(
        "${backend_header}"
        "CreateDevice("
        "Render backends must expose a uniform CreateDevice factory")
    aero_require_text(
        "${backend_header}"
        "CreateTarget("
        "Render backends must expose a uniform CreateTarget factory")
endforeach()
aero_forbid_text(
    "include/AeroRender/D3D11.hpp"
    "CreateD3D11"
    "D3D11 backend factories must not repeat the namespace name")
aero_forbid_text(
    "include/AeroRender/OpenGL33.hpp"
    "CreateOpenGL33"
    "OpenGL backend factories must not repeat the namespace name")

aero_require_text(
    "include/Aero/View.hpp"
    "bool Update(double timeInSeconds) noexcept;"
    "View must report whether an immutable frame was committed")
aero_forbid_text(
    "include/Aero/View.hpp"
    "Result<void> Update("
    "View must not expose a Result-returning per-frame update")
aero_require_text(
    "include/Aero/IRenderer.hpp"
    "virtual bool UpdateRenderTree() noexcept = 0;"
    "Renderer tree synchronization must be Result-free")
aero_require_text(
    "include/Aero/IRenderer.hpp"
    "virtual bool RenderOffscreen() noexcept = 0;"
    "Renderer offscreen execution must be Result-free")
aero_require_text(
    "include/Aero/IRenderer.hpp"
    "virtual void Render(RenderTarget& target) noexcept = 0;"
    "Renderer onscreen execution must be Result-free")

include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")
foreach(public_header IN LISTS AERO_PUBLIC_HEADERS)
    aero_require_file("${public_header}")
    file(READ "${AERO_SOURCE_DIR}/${public_header}" public_content)
    if(public_content MATCHES "#[ \t]*include[ \t]*[<\"](\\.\\./)*src/" OR
       public_content MATCHES "Aero/Integration(/|[.]hpp)" OR
       public_content MATCHES "Aero::Integration")
        message(FATAL_ERROR
            "Installed header leaks a source/private or retired Integration contract: ${public_header}")
    endif()
    if(public_content MATCHES
            "(^|[^A-Za-z0-9_])Impl([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR
            "Installed header exposes the retired Impl vocabulary: ${public_header}")
    endif()
    if(public_content MATCHES "AERO_INTERNAL_")
        message(FATAL_ERROR
            "Installed header exposes an internal build contract: ${public_header}")
    endif()
    if(public_content MATCHES
            "namespace[ \t]+[A-Za-z0-9_:]*Detail([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR
            "Installed header exposes a retired Detail namespace: ${public_header}")
    endif()
    if(public_content MATCHES
            "namespace[ \t]+[A-Za-z0-9_:]*Runtime([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR
            "Installed header exposes a retired Runtime namespace: ${public_header}")
    endif()
endforeach()

aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "enum class FontWeight"
    "FrameworkElement must not own text-formatting declarations")
aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "class AERO_GUI_API FontFamily"
    "FrameworkElement must not own media font declarations")
aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "TypeTraits<Base::Color>"
    "FrameworkElement must not own built-in value metadata")
aero_forbid_text(
    "include/Aero/Media/Brushes.hpp"
    "TypeTraits<Base::Rect>"
    "Brushes must not own built-in value metadata")

foreach(required_architecture_document IN ITEMS
        "docs/ARCHITECTURE.md"
        "docs/SOURCE_ARCHITECTURE.md"
        "docs/WINDOW_HOSTING.md")
    aero_require_file("${required_architecture_document}")
endforeach()
aero_require_file("cmake/CheckWindowsExports.cmake")
aero_require_text(
    "cmake/AeroCompilerOptions.cmake"
    "function(aero_verify_windows_exports target expected_export)"
    "Shared Windows builds must validate their real DLL export tables")
if(EXISTS "${AERO_SOURCE_DIR}/tests/CMakeLists.txt")
    aero_forbid_text(
        "tests/CMakeLists.txt"
        "Aero::Integration"
        "Framework tests must link the final Gui product rather than retired Integration")
endif()
if(EXISTS "${AERO_SOURCE_DIR}/tests/FrameworkConformanceTests.cpp")
    aero_forbid_text(
        "tests/FrameworkConformanceTests.cpp"
        "Aero/Integration"
        "Framework tests must consume the current installed SDK")
endif()

file(GLOB_RECURSE aero_physical_public_headers
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h"
    "${AERO_SOURCE_DIR}/include/AeroApp/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroApp/*.h"
    "${AERO_SOURCE_DIR}/include/AeroAudio/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroAudio/*.h"
    "${AERO_SOURCE_DIR}/include/AeroRender/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroRender/*.h")
list(SORT aero_physical_public_headers)
set(aero_declared_public_headers ${AERO_PUBLIC_HEADERS})
list(SORT aero_declared_public_headers)
if(NOT aero_physical_public_headers STREQUAL aero_declared_public_headers)
    message(FATAL_ERROR
        "Physical public headers must exactly match AERO_PUBLIC_HEADERS")
endif()

# ---------------------------------------------------------------------------
# Source ownership
# ---------------------------------------------------------------------------
foreach(retired_source_entry IN ITEMS
        "src/integration"
        "src/runtime"
        "src/providers"
        "src/platform"
        "src/controls"
        "src/diagnostics"
        "src/input"
        "src/markup"
        "src/media"
        "src/text"
        "src/gui/private"
        "src/gui/controls/private"
        "src/gui/markup/private"
        "src/gui/media/private"
        "src/render/private"
        "src/render/DeviceRenderer.cpp"
        "src/render/DeviceRenderer.hpp"
        "src/render/GraphicsDevice.cpp"
        "src/render/GraphicsDevice.hpp"
        "src/render/GraphicsDeviceResources.cpp"
        "src/render/Renderer.cpp"
        "src/render/Renderer.hpp"
        "src/render/RenderPrivate.hpp"
        "src/render/RenderSurface.cpp"
        "src/render/private/RenderSurface.hpp")
    aero_forbid_file("${retired_source_entry}")
endforeach()

foreach(required_source_entry IN ITEMS
        "src/gui/Gui.cpp"
        "src/gui/View.cpp"
        "src/gui/ViewFrame.cpp"
        "src/gui/ViewInput.cpp"
        "src/gui/ViewFocus.cpp"
        "src/gui/ViewRender.cpp"
        "src/gui/ViewRenderer.hpp"
        "src/gui/ViewState.hpp"
        "src/gui/core"
        "src/gui/meta"
        "src/gui/data"
        "src/gui/styles"
        "src/gui/controls"
        "src/gui/diagnostics"
        "src/gui/documents"
        "src/gui/shapes"
        "src/gui/input"
        "src/gui/triggers"
        "src/gui/markup"
        "src/gui/media"
        "src/gui/text"
        "src/gui/templates"
        "src/gui/markup/XamlProvider.cpp"
        "src/gui/markup/XamlReader.cpp"
        "src/gui/input/Clipboard.cpp"
        "src/gui/input/OverlayHost.cpp"
        "src/gui/input/OverlayHost.hpp"
        "src/gui/input/FocusHost.hpp"
        "src/gui/styles/ResourceHost.hpp"
        "src/render/RenderDevice.cpp"
        "src/render/RenderTarget.cpp"
        "src/render/FrameEncoder.cpp"
        "src/render/TextRenderer.cpp"
        "src/render/RenderTree.cpp"
        "src/gui/ViewRenderer.cpp"
        "src/gui/media/StoryboardHost.cpp"
        "src/gui/media/StoryboardHost.Actions.cpp"
        "src/gui/media/StoryboardHost.Completions.cpp"
        "src/gui/media/StoryboardHost.Events.cpp"
        "src/gui/core/LayoutEngine.cpp"
        "src/gui/data/BindingEngine.hpp"
        "src/gui/styles/StyleEngine.hpp"
        "src/gui/interactivity/InteractivityEngine.cpp"
        "src/gui/interactivity/InteractivityEngine.Behaviors.cpp"
        "src/gui/interactivity/InteractivityEngine.Triggers.cpp"
        "src/gui/interactivity/InteractivityEngine.Style.cpp"
        "src/gui/controls/VisualStateManager.cpp"
        "src/gui/controls/VisualStateManagerImpl.hpp"
        "src/gui/internal"
        "src/gui/internal/AeroGuiInternal.hpp"
        "src/gui/internal/AeroGuiInternal.Layout.hpp"
        "src/gui/internal/AeroGuiInternal.Visual.hpp"
        "src/gui/internal/AeroGuiInternal.Control.hpp"
        "src/gui/internal/AeroGuiInternal.Property.hpp"
        "src/gui/ViewDocuments.cpp"
        "src/gui/documents/Documents.cpp"
        "src/gui/shapes/Path.cpp"
        "src/gui/shapes/Shapes.cpp"
        "src/render/RenderContext.hpp")
    aero_require_file("${required_source_entry}")
endforeach()
file(GLOB aero_root_source_files
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
if(aero_root_source_files)
    message(FATAL_ERROR
        "Source files must belong to a real domain directory under src/: ${aero_root_source_files}")
endif()
# Final source-wide vocabulary checks. These are deliberately global so a
# retired namespace/include cannot reappear in an unlisted translation unit.
file(GLOB_RECURSE aero_source_contract_files
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.h"
    "${AERO_SOURCE_DIR}/src/*.inl"
    "${AERO_SOURCE_DIR}/src/*.inc")
foreach(source_contract_file IN LISTS aero_source_contract_files)
    get_filename_component(source_contract_name
        "${source_contract_file}" NAME)
    file(RELATIVE_PATH source_contract_relative
        "${AERO_SOURCE_DIR}" "${source_contract_file}")
    if(source_contract_name MATCHES "(Internal|Private)"
       AND NOT source_contract_relative MATCHES "^src/gui/internal/")
        message(FATAL_ERROR
            "Retired Internal/Private source filename remains: ${source_contract_relative}")
    endif()
    file(READ "${source_contract_file}" source_contract_content)
    string(FIND "${source_contract_content}"
        "Aero::Runtime::Detail" runtime_detail_position)
    string(FIND "${source_contract_content}"
        "namespace Aero::Runtime" runtime_namespace_position)
    if(NOT runtime_detail_position EQUAL -1 OR
       NOT runtime_namespace_position EQUAL -1)
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
        message(FATAL_ERROR
            "Retired Runtime namespace remains: ${source_contract_relative}")
    endif()
    if(source_contract_content MATCHES
            "namespace[ \t]+[A-Za-z0-9_:]*Detail([^A-Za-z0-9_]|$)" OR
       source_contract_content MATCHES
            "namespace[ \t]+[A-Za-z0-9_:]*Runtime([^A-Za-z0-9_]|$)")
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
        message(FATAL_ERROR
            "Retired Detail/Runtime namespace remains: ${source_contract_relative}")
    endif()
    if(source_contract_content MATCHES
            "#[ \t]*include[ \t]*[<\"][^>\"]*RenderPrivate[.]hpp[>\"]")
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
        message(FATAL_ERROR
            "Retired RenderPrivate include remains: ${source_contract_relative}")
    endif()
    if(source_contract_content MATCHES
            "AERO_(BASE|AUDIO|GUI|APP)_API")
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
        message(FATAL_ERROR
            "Source implementation must not carry an API export macro: ${source_contract_relative}")
    endif()
    if(source_contract_content MATCHES
            "(^|[^A-Za-z0-9_])Impl([^A-Za-z0-9_]|$)")
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
        message(FATAL_ERROR
            "Retired Impl vocabulary remains in source: ${source_contract_relative}")
    endif()
    foreach(retired_source_vocabulary IN ITEMS
            "View::Operations" "ViewDetail" "GuiPrivate::Detail"
            "Controls::Detail" "Markup::Detail" "Media::Detail"
            "Render::Detail" "App::Detail" "Text::Detail"
            "using namespace Detail;"
            "rendererToken" "ReleaseRenderer" "CommandQueue"
            "SubmissionStep" "submissionSteps")
        string(FIND "${source_contract_content}"
            "${retired_source_vocabulary}" retired_vocabulary_position)
        if(NOT retired_vocabulary_position EQUAL -1)
            file(RELATIVE_PATH source_contract_relative
                "${AERO_SOURCE_DIR}" "${source_contract_file}")
            message(FATAL_ERROR
                "Retired source vocabulary '${retired_source_vocabulary}' remains: ${source_contract_relative}")
        endif()
    endforeach()
endforeach()

file(GLOB aero_gui_root_files
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/src/gui/*.cpp"
    "${AERO_SOURCE_DIR}/src/gui/*.hpp")
set(aero_allowed_gui_root_files
    "src/gui/Gui.cpp"
    "src/gui/GuiData.hpp"
    "src/gui/View.cpp"
    "src/gui/ViewFrame.cpp"
    "src/gui/ViewInput.cpp"
    "src/gui/ViewFocus.cpp"
    "src/gui/ViewRender.cpp"
    "src/gui/ViewState.hpp"
    "src/gui/ViewRenderer.hpp"
    "src/gui/ViewRenderer.cpp"
    "src/gui/ViewDocuments.cpp")
foreach(aero_gui_root_file IN LISTS aero_gui_root_files)
    if(NOT aero_gui_root_file IN_LIST aero_allowed_gui_root_files)
        message(FATAL_ERROR
            "src/gui root is reserved for Gui/View composition files: ${aero_gui_root_file}")
    endif()
endforeach()

file(READ "${AERO_SOURCE_DIR}/cmake/AeroGuiTargets.cmake"
    aero_gui_target_source_text)
file(GLOB_RECURSE aero_gui_cpp_sources
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/src/gui/*.cpp")
foreach(aero_gui_cpp_source IN LISTS aero_gui_cpp_sources)
    string(REGEX MATCHALL "${aero_gui_cpp_source}"
        aero_gui_source_owners "${aero_gui_target_source_text}")
    list(LENGTH aero_gui_source_owners aero_gui_source_owner_count)
    if(NOT aero_gui_source_owner_count EQUAL 1)
        message(FATAL_ERROR
            "Gui source must have exactly one explicit AeroGui compile owner: ${aero_gui_cpp_source} (${aero_gui_source_owner_count})")
    endif()
endforeach()

aero_forbid_text(
    "include/Aero/Gui.hpp" "AddXamlProvider"
    "The retired host-owned XAML provider API must not return")
aero_forbid_text(
    "include/Aero/Gui.hpp" "AddTextureProvider"
    "The retired host-owned texture provider API must not return")
aero_forbid_text(
    "include/Aero/Gui.hpp" "AddFontProvider"
    "The retired host-owned font provider API must not return")
aero_require_text(
    "include/Aero/Gui.hpp" "Ref<Markup::XamlProvider> provider"
    "Gui must strongly own configured XAML providers")
aero_require_text(
    "include/Aero/Gui.hpp" "Ref<Media::TextureProvider> provider"
    "Gui must strongly own the texture provider")
aero_require_text(
    "include/Aero/Gui.hpp" "Ref<Media::FontProvider> provider"
    "Gui must strongly own the font provider")
aero_forbid_text(
    "include/Aero/Markup/XamlProvider.hpp" "CacheIdentity"
    "Provider cache identity must remain registry-private")
aero_forbid_text(
    "include/Aero/Media/TextureProvider.hpp" "CacheIdentity"
    "Provider cache identity must remain registry-private")
aero_forbid_text(
    "src/gui/GuiData.hpp" "XamlProvider*"
    "Gui provider ownership must not use raw XAML pointers")
aero_forbid_text(
    "src/gui/GuiData.hpp" "TextureProvider*"
    "Gui provider ownership must not use raw texture pointers")
aero_forbid_text(
    "src/gui/GuiData.hpp" "FontProvider*"
    "Gui provider ownership must not use raw font pointers")
file(GLOB_RECURSE aero_provider_api_consumers
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/include/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/samples/*.hpp"
    "${AERO_SOURCE_DIR}/samples/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.cpp"
    "${AERO_SOURCE_DIR}/tests/*.hpp"
    "${AERO_SOURCE_DIR}/tests/*.cpp")
foreach(aero_provider_api_consumer IN LISTS aero_provider_api_consumers)
    file(READ
        "${AERO_SOURCE_DIR}/${aero_provider_api_consumer}"
        aero_provider_api_content)
    if(aero_provider_api_content MATCHES
            "Add(Xaml|Texture|Font)Provider[ \\t\\r\\n]*[(]")
        message(FATAL_ERROR
            "Retired Provider API remains: ${aero_provider_api_consumer}")
    endif()
endforeach()
unset(aero_provider_api_consumers)
unset(aero_provider_api_consumer)
unset(aero_provider_api_content)
unset(aero_source_contract_files)
unset(source_contract_content)
unset(source_contract_relative)
unset(runtime_detail_position)
unset(runtime_namespace_position)
unset(retired_source_vocabulary)
unset(retired_vocabulary_position)
unset(source_contract_name)

# ---------------------------------------------------------------------------
# Rendering ownership
# ---------------------------------------------------------------------------
aero_require_file("src/gui/ViewRenderer.hpp")
aero_require_text(
    "src/gui/ViewRenderer.hpp"
    "class ViewRenderer final : public IRenderer"
    "View must own the only concrete IRenderer")
aero_require_text(
    "src/gui/ViewRenderer.hpp"
    "std::optional<::Aero::Render::UiFrameEncoder> frameEncoder_"
    "ViewRenderer must directly own its delayed frame encoder")
aero_forbid_text(
    "src/gui/ViewRenderer.hpp"
    "struct Access"
    "ViewRenderer must use direct members rather than a source-only pimpl")
aero_require_text(
    "src/gui/ViewRenderer.hpp"
    "RenderOnscreenFrame("
    "ViewRenderer must own onscreen UI rendering")
aero_require_text(
    "src/gui/ViewRenderer.hpp"
    "RenderOffscreenFrame("
    "ViewRenderer must own offscreen UI rendering")
aero_forbid_text(
    "src/gui/ViewRenderer.hpp"
    "SurfaceSession"
    "Renderer must not depend on a second surface lifecycle")
aero_forbid_file("src/render/Surface.hpp")
aero_forbid_file("src/render/Surface.cpp")
aero_forbid_file("src/render/WindowRenderContext.hpp")
aero_require_file("src/render/Presentation.hpp")
aero_forbid_text(
    "src/render/Presentation.hpp"
    "Descriptor"
    "Render presentation values must not recreate a generic native-context protocol")
aero_forbid_text(
    "src/render/d3d11/D3D11Device.cpp"
    "ViewRenderer"
    "Backend target implementations must not depend on the private GUI renderer")
aero_forbid_text(
    "src/render/d3d11/D3D11Device.cpp"
    "RenderOnscreenFrame"
    "AeroGui must own onscreen frame submission")
aero_forbid_file("src/render/private/BackendApi.hpp")
aero_forbid_file("src/render/RenderTargetState.hpp")
aero_forbid_file("src/render/RenderDeviceState.hpp")
aero_forbid_file("src/render/GraphicsTypes.hpp")
aero_forbid_file("src/render/RenderBatch.hpp")
aero_forbid_file("src/render/RenderBatch.cpp")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "GraphicsDevice"
    "The retired generic graphics device must not be built")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "using FrameEncoder ="
    "The low-level command encoder must not be exposed through a migration alias")
aero_require_text(
    "src/render/FrameEncoder.hpp"
    "class UiFrameEncoder"
    "The low-level helper must be an explicit UI frame encoder")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "class Renderer {"
    "FrameEncoder must not recreate a peer semantic Renderer")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "using RenderTarget ="
    "The frame attachment value must not shadow the SDK RenderTarget")

aero_require_text(
    "src/render/opengl33/OpenGL33RenderContext.cpp"
    "class OpenGLRenderContext final : public RenderContext"
    "OpenGL window presentation must belong to a concrete render backend context")
aero_forbid_text(
    "src/render/d3d11/D3D11Device.cpp"
    "ViewRenderData*"
    "D3D11 RenderDevice must not own per-View renderer data")
aero_forbid_text(
    "src/render/opengl33/OpenGL33RenderDevice.hpp"
    "ViewRenderData*"
    "OpenGL RenderDevice must not own per-View renderer data")
aero_forbid_file("src/render/opengl33/OpenGL33Embedded.cpp")
aero_forbid_text(
    "src/render/FrameEncoder.cpp"
    "using Renderer ="
    "UI frame encoding must not retain a local Renderer migration alias")
aero_forbid_text(
    "src/render/FrameEncoder.cpp"
    "using RenderTarget ="
    "UI frame encoding must use FrameTarget directly")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "BatchComposer"
    "ViewRenderer must not retain a second BatchComposer owner")
aero_forbid_file("src/render/opengl33/OpenGL33Device.cpp")
aero_forbid_text(
    "src/render/RenderTree.hpp"
    "RenderDevice"
    "RenderTree must only build immutable frames")
aero_forbid_text(
    "src/render/RenderTree.hpp"
    "Submit("
    "RenderTree must not own GPU submission")

aero_require_text(
    "src/render/RenderContext.hpp"
    "Base::Result<void> presented = PresentFrame();"
    "RenderContext must own the final desktop Present boundary")
aero_require_text(
    "src/render/RenderContext.hpp"
    "bool frameOpen_ = false;"
    "RenderContext must own desktop frame state")
aero_require_text(
    "src/render/d3d11/D3D11RenderContext.cpp"
    "class D3D11RenderContext final : public RenderContext"
    "D3D11 window/device creation must belong to a concrete RenderContext")
aero_require_text(
    "src/render/opengl33/OpenGL33RenderContext.cpp"
    "class OpenGLRenderContext final : public RenderContext"
    "OpenGL window/device creation must belong to a concrete RenderContext")
aero_forbid_text(
    "src/render/RenderContext.hpp"
    "class RenderContext final"
    "RenderContext must remain the shared presentation lifecycle base")
aero_forbid_file("src/render/RenderCommands.hpp")
aero_forbid_file("src/render/RenderCommands.cpp")
aero_forbid_text(
    "src/render/d3d11/D3D11RenderDevice.cpp"
    "DrawPhase"
    "D3D11 must execute one Batch directly without an internal command model")
aero_forbid_text(
    "src/render/opengl33/OpenGL33RenderDevice.cpp"
    "DrawPhase"
    "OpenGL must execute one Batch directly without an internal command model")
aero_require_text(
    "src/gui/ViewRenderer.hpp"
    "RenderOnscreenFrame("
    "ViewRenderer must submit the source-private UI draws")
aero_forbid_file("src/render/BatchPlanner.cpp")
aero_forbid_file("src/render/BatchPlanner.hpp")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "BatchPlanner"
    "The duplicate diagnostic batching pipeline must not be part of AeroGui")
aero_forbid_text(
    "src/render/d3d11/D3D11RenderContext.cpp"
    "CreateD3D11WindowSurface"
    "Desktop D3D11 hosting must explicitly compose device and target")
aero_forbid_text(
    "src/render/opengl33/OpenGL33RenderContext.cpp"
    "CreateOpenGL33WindowSurface"
    "Desktop OpenGL hosting must explicitly compose device and target")

# ---------------------------------------------------------------------------
# WPF code-behind experience
# ---------------------------------------------------------------------------
aero_require_text(
    "include/AeroApp/Window.hpp"
    "void InitializeComponent() noexcept;"
    "Window code-behind must expose the conventional InitializeComponent entry")
aero_forbid_file("src/app/ApplicationRun.cpp")
aero_forbid_file("src/app/RenderContextFactory.hpp")
aero_forbid_file("src/app/RenderContextFactory.cpp")
aero_require_text(
    "include/Aero/Meta.hpp"
    "TypeBuilder& EventHandler("
    "Component metadata must expose a direct XAML event-handler description")
aero_require_text(
    "include/Aero/Meta.hpp"
    "DefineComponentModule("
    "Custom components must not require handwritten Registry or facet adapters")
aero_require_text(
    "include/Aero/Meta.hpp"
    "class TypeBuilder"
    "Public metadata authoring must use the compact TypeBuilder spelling")
aero_require_text(
    "include/Aero/Meta.hpp"
    "class FrameworkPropertyMetadata"
    "Dependency-property authoring must expose typed framework metadata")
aero_forbid_text(
    "include/Aero/Meta.hpp"
    "TypeDescription"
    "The legacy metadata description surface must remain retired")
aero_forbid_text(
    "include/Aero/Meta.hpp"
    "PropertyOptions"
    "The legacy property-options surface must remain retired")
aero_require_text(
    "tools/sdk-consumers/MetaConsumer.cpp"
    "Aero::Meta::FrameworkPropertyMetadata("
    "The SDK consumer must compile the concise framework metadata form")
aero_require_text(
    "tools/sdk-consumers/MetaConsumer.cpp"
    "Aero::Meta::AffectsRender"
    "The SDK consumer must compile concise metadata option constants")
aero_require_text(
    "include/Aero/DependencyProperty.hpp"
    "#if defined(AERO_GUI_IMPLEMENTATION)\nclass AERO_GUI_API DependencyPropertyRegistry"
    "Mutable dependency-property registry storage must remain implementation-only")
aero_require_text(
    "tools/sdk-consumers/MetaConsumer.cpp"
    "!HasPropertyRegistry<Aero::DependencyObject>::value"
    "SDK consumers must prove DependencyObject does not expose its runtime registry")
aero_require_text(
    "tools/sdk-consumers/MetaConsumer.cpp"
    "!HasRawFactoryOverload<ConsumerControl>::value"
    "SDK consumers must prove metadata callback ABI is absent from the typed facade")
file(READ
    "${AERO_SOURCE_DIR}/include/Aero/Meta.hpp"
    aero_meta_header_text)
string(FIND
    "${aero_meta_header_text}"
    "template<class T>\nclass TypeBuilder {"
    aero_meta_facade_offset)
if(aero_meta_facade_offset EQUAL -1)
    message(FATAL_ERROR "Meta.hpp: TypeBuilder facade marker is missing")
endif()
string(SUBSTRING
    "${aero_meta_header_text}"
    ${aero_meta_facade_offset}
    -1
    aero_meta_facade_text)
foreach(aero_low_level_spelling IN ITEMS
        "Base::Ref"
        "Base::Result"
        "Base::String"
        "Base::StringView"
        "Base::Span"
        "Base::Object")
    string(FIND
        "${aero_meta_facade_text}"
        "${aero_low_level_spelling}"
        aero_meta_low_level_spelling_offset)
    if(NOT aero_meta_low_level_spelling_offset EQUAL -1)
        message(FATAL_ERROR
            "Meta.hpp: typed authoring facade exposes ${aero_low_level_spelling}")
    endif()
endforeach()
foreach(aero_private_access_header IN ITEMS
        "include/Aero/Controls/Control.hpp"
        "include/Aero/Media/Brushes.hpp")
    aero_require_text(
        "${aero_private_access_header}"
        "#if defined(AERO_GUI_IMPLEMENTATION)"
        "Access implementation seams must be private to SDK consumers")
endforeach()
file(GLOB_RECURSE aero_public_headers_with_access
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroApp/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroAudio/*.hpp"
    "${AERO_SOURCE_DIR}/include/AeroRender/*.hpp")
foreach(aero_public_header IN LISTS aero_public_headers_with_access)
    file(READ
        "${AERO_SOURCE_DIR}/${aero_public_header}"
        aero_public_header_text)
    string(REGEX MATCH
        "public:[ \t\r\n]+struct Access;"
        aero_public_access_match
        "${aero_public_header_text}")
    if(NOT aero_public_access_match STREQUAL "")
        message(FATAL_ERROR
            "${aero_public_header}: Access implementation seam is public to SDK consumers")
    endif()
endforeach()
foreach(aero_render_contract_header IN ITEMS
        "include/AeroRender/RenderDevice.hpp"
        "include/AeroRender/RenderTarget.hpp")
    aero_forbid_text(
        "${aero_render_contract_header}"
        "struct Access"
        "Render contracts must not expose the retired Access seam")
endforeach()
aero_require_text(
    "src/render/d3d11/D3D11RenderDevice.hpp"
    "public Aero::Render::RenderDeviceBase"
    "D3D11 must implement the core RenderDevice contract directly")
aero_require_text(
    "src/render/opengl33/OpenGL33RenderDevice.hpp"
    "public Aero::Render::RenderDeviceBase"
    "OpenGL33 must implement the core RenderDevice contract directly")
aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "struct Access;"
    "FrameworkElement must not expose an unused Access seam")
aero_require_text(
    "src/gui/markup/XamlObjectWriter.cpp"
    "ObjectBuilder::ConnectEvent("
    "XAML event attributes must connect through the object-writer pipeline")
aero_require_file("templates/AeroApp/App.xaml")
aero_require_file("templates/AeroApp/MainWindow.xaml")

# ---------------------------------------------------------------------------
# Gui / XAML / View ownership
# ---------------------------------------------------------------------------
aero_require_text(
    "include/Aero/Markup/XamlReader.hpp"
    "explicit XamlReader(Aero::Gui& gui)"
    "XamlReader must be Gui-owned rather than View-owned")
aero_forbid_text(
    "include/Aero/Markup/XamlReader.hpp"
    "XamlReader(Aero::View&"
    "XamlReader must not recreate View-owned loading")
aero_forbid_text(
    "include/Aero/View.hpp"
    "friend class Markup::ReloadCoordinator"
    "ReloadCoordinator must not require View private access")
aero_forbid_text(
    "include/Aero/View.hpp"
    "QueryReloadSource("
    "Reload source/cache ownership belongs to Gui/Markup")
aero_require_text(
    "include/Aero/View.hpp"
    "Result<void> SetViewport(const ViewViewport& viewport) noexcept;"
    "Installed View must support atomic logical/pixel/DPI viewport updates")
aero_require_text(
    "include/Aero/View.hpp"
    "void SetSize("
    "Installed View must retain the Noesis-friendly SetSize facade")
aero_require_text(
    "include/Aero/View.hpp"
    "void SetScale(double scale) noexcept;"
    "Installed View must retain the Noesis-friendly SetScale facade")
aero_forbid_text(
    "include/Aero/View.hpp"
    "DispatchPointer("
    "Installed View must expose host-friendly pointer methods")
aero_forbid_text(
    "include/Aero/View.hpp"
    "DispatchKeyboard("
    "Installed View must expose host-friendly keyboard methods")
aero_forbid_text(
    "include/Aero/View.hpp"
    "DispatchText("
    "Installed View must expose Char rather than a generic text dispatch protocol")
foreach(view_private_operation IN ITEMS
        "MountContent(" "UnmountContent(" "LoadResources("
        "LoadCompiledResources(" "SetResourceDictionary("
        "LoadBuiltInTheme(" "ExecuteFrame(" "AdvanceClocks("
        "AdvanceAnimations(" "FindNamedObject(" "NamedObjectCount("
        "IsInstanceOf(")
    aero_forbid_text(
        "include/Aero/View.hpp"
        "${view_private_operation}"
        "View implementation operations must remain source-only")
endforeach()
aero_require_text(
    "src/gui/markup/ReloadCoordinator.cpp"
    "gui->xaml.QuerySource"
    "ReloadCoordinator must query the Gui-owned XAML runtime directly")
aero_require_file("src/gui/ViewRenderer.hpp")
aero_forbid_file("cmake/AeroRuntimeTargets.cmake")
aero_forbid_file("cmake/AeroGuiRuntimeTargets.cmake")
aero_forbid_file("cmake/AeroGuiCompositionTargets.cmake")
aero_forbid_file("cmake/AeroRenderingTargets.cmake")
aero_forbid_text(
    "include/Aero/View.hpp"
    "Runtime::Detail"
    "View must not expose a generic Runtime implementation namespace")
aero_forbid_text(
    "src/gui/media/ImageCache.hpp"
    "Runtime::Detail"
    "ImageCache belongs to Media rather than a generic Runtime namespace")
aero_forbid_text(
    "src/gui/text/TextPipeline.hpp"
    "Runtime::Detail"
    "TextPipeline belongs to Text rather than a generic Runtime namespace")
aero_forbid_text(
    "src/gui/text/TextPipeline.cpp"
    "RenderSurface.hpp"
    "TextPipeline must not depend on the retired RenderSurface contract")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "AERO_INTERNAL_RUNTIME"
    "Gui composition must not recreate the generic Runtime product spelling")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "AERO_INTERNAL_GUI_RUNTIME"
    "Gui composition must use composition rather than Runtime build vocabulary")
aero_forbid_file("tools/sdk-consumers/GuiRuntimeConsumer.cpp")
aero_require_file("tools/sdk-consumers/GuiCompositionConsumer.cpp")
aero_forbid_text(
    "src/gui/View.cpp"
    "Aero::Runtime::Detail"
    "View must not depend on the retired generic Runtime namespace")
aero_forbid_text(
    "src/gui/templates/DataTemplateTriggerState.hpp"
    "Aero::Runtime::Detail"
    "DataTemplate trigger state belongs to Templates")
aero_forbid_text(
    "src/gui/View.cpp"
    "controls/ControlsPrivate.hpp"
    "View must include the private control contracts it actually consumes")
aero_forbid_text(
    "src/gui/View.cpp"
    "gui/GuiPrivate.hpp"
    "View must not import the whole private Gui domain")
aero_forbid_text(
    "src/gui/View.cpp"
    "media/MediaPrivate.hpp"
    "View must not import the whole private Media domain")
aero_forbid_file("src/gui/GuiPrivate.hpp")
aero_forbid_file("src/gui/controls/ControlsPrivate.hpp")
aero_forbid_file("src/gui/markup/MarkupPrivate.hpp")
aero_forbid_file("src/gui/media/MediaPrivate.hpp")
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/media/StoryboardHost.Events.cpp"
    "EventTrigger runtime must compile with StoryboardHost")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/interactivity/InteractivityEngine.Events.cpp"
    "EventTrigger runtime must not remain on InteractivityEngine")
aero_forbid_file("src/gui/interactivity/ViewTriggers.cpp")
aero_forbid_file("src/gui/interactivity/InteractivityEngine.Events.cpp")
aero_forbid_file("src/gui/media/ViewStoryboardSessions.cpp")
aero_forbid_file("src/gui/controls/Layout.cpp")
aero_forbid_file("src/gui/markup/ViewDocuments.cpp")
aero_forbid_file("src/gui/controls/Documents.cpp")
aero_forbid_file("src/gui/controls/Path.cpp")
aero_forbid_file("src/gui/controls/Shapes.cpp")
aero_forbid_file("src/gui/data/BindingState.hpp")
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "_aero_gui_interactivity_sources"
    "Blend/interactivity TUs must be a dedicated AeroGui source group")
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "_aero_gui_documents_sources"
    "Documents.cpp must compile in a documents source group")
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "_aero_gui_shapes_sources"
    "Path.cpp and Shapes.cpp must compile in a shapes source group")
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/ViewDocuments.cpp"
    "ViewDocuments must compile with View composition sources")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/markup/ViewDocuments.cpp"
    "ViewDocuments must not remain in the markup source group")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/controls/Documents.cpp"
    "Documents.cpp must not remain in the controls source group")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/controls/Path.cpp"
    "Path.cpp must not remain in the controls source group")
aero_forbid_text(
    "cmake/AeroGuiTargets.cmake"
    "src/gui/controls/Shapes.cpp"
    "Shapes.cpp must not remain in the controls source group")
aero_require_text(
    "src/gui/data/BindingEngine.hpp"
    "class BindingEngine"
    "BindingEngine.hpp must own the BindingEngine declaration")
aero_require_text(
    "src/gui/styles/StyleEngine.hpp"
    "class StyleEngine"
    "StyleEngine.hpp must own the StyleEngine declaration")
aero_forbid_text(
    "src/gui/styles/StyleState.hpp"
    "class StyleEngine"
    "StyleEngine must not remain declared in StyleState.hpp")
aero_forbid_file("src/gui/core/Facet.hpp")
aero_forbid_file("src/gui/core/facets/VisualFacet.hpp")
aero_forbid_file("src/gui/core/facets/RenderFacet.hpp")
aero_forbid_file("src/gui/core/facets/LayoutFacet.hpp")
aero_forbid_file("src/gui/core/facets/LayoutFacets.hpp")
aero_forbid_file("src/gui/core/facets/ServiceFacets.hpp")
aero_forbid_file("src/gui/core/facets/InteractionStateFacet.hpp")
aero_forbid_file("src/gui/core/facets/DependencyPropertyFacet.hpp")
aero_forbid_file("src/gui/core/facets/InputEventFacet.hpp")
aero_forbid_file("src/gui/core/facets/TextLayoutFacet.hpp")
aero_require_text(
    "include/Aero/Controls/Button.hpp"
    "class AERO_GUI_API Button"
    "Button.hpp must own the Button declaration")
aero_require_text(
    "include/Aero/FrameworkElement.hpp"
    "Result<ResourceValue> FindResource(const ResourceKey& key) const noexcept"
    "FrameworkElement must expose WPF-shaped FindResource")
aero_require_text(
    "include/Aero/FrameworkElement.hpp"
    "Result<ResourceValue> TryFindResource(const ResourceKey& key) const noexcept"
    "FrameworkElement must expose WPF-shaped TryFindResource")
aero_forbid_text(
    "include/Aero/Controls/RangeBase.hpp"
    "class AERO_GUI_API Slider"
    "RangeBase.hpp must not declare Slider; use Controls/Slider.hpp")
aero_forbid_text(
    "include/Aero/Controls/RangeBase.hpp"
    "class AERO_GUI_API Thumb"
    "RangeBase.hpp must not declare Thumb; use Primitives/Thumb.hpp")
aero_forbid_text(
    "include/Aero/Controls/RangeBase.hpp"
    "class AERO_GUI_API ProgressBar"
    "RangeBase.hpp must not declare ProgressBar; use Controls/ProgressBar.hpp")
aero_forbid_text(
    "include/Aero/Controls/RangeBase.hpp"
    "class AERO_GUI_API GridSplitter"
    "RangeBase.hpp must not declare GridSplitter; use Controls/GridSplitter.hpp")
aero_forbid_text(
    "include/Aero/Controls/RangeBase.hpp"
    "class AERO_GUI_API ScrollBar"
    "RangeBase.hpp must not declare ScrollBar; use Primitives/ScrollBar.hpp")
aero_require_text(
    "include/Aero/Controls.hpp"
    "#include <Aero/Controls/Slider.hpp>"
    "Controls.hpp must include the split Slider header")
aero_forbid_file("include/Aero/Gui/Primitives.hpp")
aero_forbid_text(
    "include/Aero/Controls/ControlTemplate.hpp"
    "class AERO_GUI_API VisualState"
    "VisualState public types must live in VisualStateManager.hpp")
aero_forbid_text(
    "include/Aero/Controls/ControlTemplate.hpp"
    "class AERO_GUI_API VisualStateManager"
    "VisualStateManager must not be declared in ControlTemplate.hpp")
aero_require_text(
    "include/Aero/VisualStateManager.hpp"
    "static bool GoToState("
    "VisualStateManager must keep the public WPF GoToState entry")
aero_forbid_text(
    "include/Aero/Triggers/Triggers.hpp"
    "Interactivity/"
    "Triggers.hpp must not include Blend Interactivity headers")
aero_forbid_text(
    "include/Aero/Triggers/Triggers.hpp"
    "Media/Animation/"
    "Triggers.hpp must not include Media.Animation trigger headers")
aero_forbid_text(
    "include/Aero/View.hpp"
    "class AERO_GUI_API CompositionTarget"
    "CompositionTarget must not be declared in View.hpp")

# S25: public headers are organized by WPF-visible type ownership. Retired
# Gui-path compatibility umbrellas are absent rather than forwarded.
foreach(s14_owner IN ITEMS
        "include/Aero/Controls/Control.hpp|class AERO_GUI_API Control"
        "include/Aero/Controls/ContentControl.hpp|class AERO_GUI_API ContentControl"
        "include/Aero/Controls/Panel.hpp|class AERO_GUI_API Panel"
        "include/Aero/Controls/Grid.hpp|class AERO_GUI_API Grid"
        "include/Aero/Controls/ListBox.hpp|class AERO_GUI_API ListBox"
        "include/Aero/Controls/ComboBox.hpp|class AERO_GUI_API ComboBox"
        "include/Aero/Controls/ListView.hpp|class AERO_GUI_API ListView"
        "include/Aero/Controls/TreeView.hpp|class AERO_GUI_API TreeView"
        "include/Aero/Controls/TextBox.hpp|class AERO_GUI_API TextBox"
        "include/Aero/Controls/Primitives/Thumb.hpp|class AERO_GUI_API Thumb"
        "include/Aero/Controls/Primitives/Track.hpp|class AERO_GUI_API Track"
        "include/Aero/Controls/Primitives/RangeBase.hpp|class AERO_GUI_API RangeBase"
        "include/Aero/Controls/Primitives/ScrollBar.hpp|class AERO_GUI_API ScrollBar"
        "include/Aero/Controls/Primitives/TickBar.hpp|class AERO_GUI_API TickBar"
        "include/Aero/Controls/Primitives/ButtonBase.hpp|class AERO_GUI_API ButtonBase"
        "include/Aero/Controls/Primitives/ToggleButton.hpp|class AERO_GUI_API ToggleButton"
        "include/Aero/Controls/Primitives/RepeatButton.hpp|class AERO_GUI_API RepeatButton"
        "include/Aero/Controls/StackPanel.hpp|class AERO_GUI_API StackPanel"
        "include/Aero/Controls/DockPanel.hpp|class AERO_GUI_API DockPanel"
        "include/Aero/Controls/WrapPanel.hpp|class AERO_GUI_API WrapPanel"
        "include/Aero/Controls/UniformGrid.hpp|class AERO_GUI_API UniformGrid"
        "include/Aero/Controls/Canvas.hpp|class AERO_GUI_API Canvas"
        "include/Aero/Controls/Border.hpp|class AERO_GUI_API Border"
        "include/Aero/Controls/Viewbox.hpp|class AERO_GUI_API Viewbox"
        "include/Aero/Controls/HeaderedContentControl.hpp|class AERO_GUI_API HeaderedContentControl"
        "include/Aero/Controls/GroupBox.hpp|class AERO_GUI_API GroupBox"
        "include/Aero/Controls/Label.hpp|class AERO_GUI_API Label"
        "include/Aero/Controls/Expander.hpp|class AERO_GUI_API Expander"
        "include/Aero/Controls/TabItem.hpp|class AERO_GUI_API TabItem"
        "include/Aero/Controls/TabControl.hpp|class AERO_GUI_API TabControl"
        "include/Aero/Controls/TabPanel.hpp|class AERO_GUI_API TabPanel"
        "include/Aero/ContentElement.hpp|class AERO_GUI_API ContentElement"
        "include/Aero/FrameworkContentElement.hpp|class AERO_GUI_API FrameworkContentElement"
        "include/Aero/DependencyObject.hpp|class AERO_GUI_API DependencyObject"
        "include/Aero/Visual.hpp|class AERO_GUI_API Visual"
        "include/Aero/VisualTreeHelper.hpp|class AERO_GUI_API VisualTreeHelper"
        "include/Aero/LogicalTreeHelper.hpp|class AERO_GUI_API LogicalTreeHelper"
        "include/Aero/Controls/Slider.hpp|class AERO_GUI_API Slider"
        "include/Aero/Controls/ProgressBar.hpp|class AERO_GUI_API ProgressBar"
        "include/Aero/Controls/GridSplitter.hpp|class AERO_GUI_API GridSplitter"
        "include/Aero/VisualStateManager.hpp|class AERO_GUI_API VisualStateManager"
        "include/Aero/FrameworkTemplate.hpp|class AERO_GUI_API FrameworkTemplate"
        "include/Aero/Media/CompositionTarget.hpp|class AERO_GUI_API CompositionTarget")
    string(REPLACE "|" ";" s14_owner_parts "${s14_owner}")
    list(GET s14_owner_parts 0 s14_owner_header)
    list(GET s14_owner_parts 1 s14_owner_declaration)
    aero_require_text(
        "${s14_owner_header}"
        "${s14_owner_declaration}"
        "S14 leaf header must own its public declaration")
endforeach()
foreach(s14_retired_umbrella IN ITEMS
        "include/Aero/Gui/Core.hpp"
        "include/Aero/Gui/Common.hpp"
        "include/Aero/Gui/Panels.hpp"
        "include/Aero/Gui/Items.hpp"
        "include/Aero/Gui/Primitives.hpp")
    aero_forbid_file("${s14_retired_umbrella}")
endforeach()
aero_forbid_file("include/Aero/Gui/Text.hpp")
aero_forbid_text(
    "include/Aero/Value.hpp"
    "struct Members"
    "WPF-facing dependency properties and routed events must not use the retired Members category")
# ---------------------------------------------------------------------------
# Public API and build model
# ---------------------------------------------------------------------------
aero_forbid_text(
    "include/AeroApp/Application.hpp"
    "RunChecked"
    "Application must expose one Result-returning Run family")
aero_forbid_text(
    "include/AeroApp/Application.hpp"
    "Run(Base::Ref<Window>"
    "Explicit windows must be supplied through SetMainWindow")
aero_forbid_text(
    "include/AeroApp/Application.hpp"
    "SetMainWindowBorrowed"
    "Application ownership adapters must remain source-only")
aero_forbid_text(
    "include/Aero/Controls/ItemsControl.hpp"
    "SetItemsSourceBorrowed"
    "ItemsControl ownership adapters must remain source-only")
aero_forbid_text(
    "include/AeroApp/Window.hpp"
    "ShowChecked"
    "Window must expose one Result-returning Show API")
aero_forbid_text(
    "include/AeroApp/Window.hpp"
    "CloseChecked"
    "Window must expose one Result-returning Close API")

aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "function(aero_compile_d3d11_shader_pair"
    "D3D11 shader compilation must use the shared helper")

file(GLOB aero_target_modules
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/cmake/*.cmake")
set(product_text "")
foreach(target_module IN LISTS aero_target_modules)
    file(READ "${target_module}" module_content)
    string(APPEND product_text "\n${module_content}")
endforeach()
foreach(required_product IN ITEMS
        "add_library(Aero::Base ALIAS AeroBase)"
        "add_library(Aero::Audio ALIAS AeroAudio)"
        "add_library(Aero::Gui ALIAS AeroGui)"
        "add_library(Aero::Render ALIAS AeroRender)"
        "add_library(Aero::RenderD3D11 ALIAS AeroRenderD3D11)"
        "add_library(Aero::RenderOpenGL33 ALIAS AeroRenderOpenGL33)"
        "add_library(Aero::App ALIAS AeroApp)")
    string(FIND "${product_text}" "${required_product}" product_position)
    if(product_position EQUAL -1)
        message(FATAL_ERROR "Required product target is missing: ${required_product}")
    endif()
endforeach()
if(product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*AeroIntegration" OR
   product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*Aero::Integration")
    message(FATAL_ERROR "The retired Integration product must not be recreated")
endif()
aero_require_text(
    "cmake/AeroProductTargets.cmake"
    "AeroGui must remain backend neutral"
    "AeroGui target validation must reject native backend sources")
aero_require_text(
    "cmake/AeroProductTargets.cmake"
    "Aero::RenderD3D11"
    "App must compose the enabled D3D11 backend product")
aero_require_text(
    "cmake/AeroProductTargets.cmake"
    "Aero::RenderOpenGL33"
    "App must compose the enabled OpenGL33 backend product")
foreach(retired_object_layer IN ITEMS
        AeroGuiKernelObjects AeroControlsObjects AeroMarkupKernelObjects
        AeroMarkupObjects AeroInspectorObjects AeroRuntimeObjects
        AeroRenderingObjects)
    if(product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*${retired_object_layer}")
        message(FATAL_ERROR
            "Product implementation object layer was recreated: ${retired_object_layer}")
    endif()
endforeach()

foreach(aero_authoritative_document IN ITEMS
        "docs/ARCHITECTURE.md"
        "docs/SDK_PACKAGING.md"
        "docs/WINDOW_HOSTING.md"
        "docs/WPF_CPP_PORT_SPEC.md"
        "docs/spec/FINAL_SDK_SURFACE.md"
        "docs/spec/METADATA_PRODUCT_MODEL.md"
        "docs/spec/PUBLIC_HEADER_MODEL.md"
        "docs/spec/PUBLIC_NAMESPACE_MODEL.md")
    aero_forbid_text(
        "${aero_authoritative_document}" "Aero::Integration"
        "Authoritative documentation must not reference the retired Integration product")
    aero_forbid_text(
        "${aero_authoritative_document}" "src/text"
        "Authoritative documentation must use the current Gui text domain")
endforeach()

# ---------------------------------------------------------------------------
# WPF kernel: no Core::Facet / Access bags. View/ElementTree is the service hub.
# XAML metadata capabilities (XamlFacets) are a different system and may remain.
# ---------------------------------------------------------------------------
aero_require_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "class AeroGuiInternal"
    "Kernel-private operations must live in src/gui/internal/AeroGuiInternal.hpp")
aero_require_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "#include \"gui/internal/AeroGuiInternal.Layout.hpp\""
    "AeroGuiInternal tree/layout members must stay in the single friend class via section include")
aero_require_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "#include \"gui/internal/AeroGuiInternal.Visual.hpp\""
    "AeroGuiInternal visual/render members must stay in the single friend class via section include")
aero_require_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "#include \"gui/internal/AeroGuiInternal.Control.hpp\""
    "AeroGuiInternal control/template members must stay in the single friend class via section include")
aero_require_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "#include \"gui/internal/AeroGuiInternal.Property.hpp\""
    "AeroGuiInternal property-store members must stay in the single friend class via section include")
aero_require_text(
    "src/gui/internal/PropertyStore.hpp"
    "struct StoredValueRare"
    "Packed StoredValueEntry must keep expression/animation/current in StoredValueRare")
aero_require_text(
    "src/gui/internal/PropertyStore.hpp"
    "StoredValueRare* rare"
    "Packed StoredValueEntry must hold an uncommon-data pointer, not six PropertyValues")
aero_forbid_text(
    "src/gui/internal/PropertyStore.hpp"
    "PropertyValue baseValue"
    "Packed StoredValueEntry must not store the unused baseValue snapshot")
aero_forbid_text(
    "src/gui/internal/PropertyStore.hpp"
    "PropertyValue localValue;\n    PropertyValue currentValue;\n    PropertyValue inheritedValue;"
    "Packed StoredValueEntry must not keep the six-copy PropertyValue layout")
aero_require_text(
    "include/Aero/DependencyObject.hpp"
    "void* valueStore_ = nullptr;"
    "Public DependencyObject ABI must keep valueStore_ as opaque void*")
aero_require_text(
    "src/gui/core/DependencyObject.cpp"
    "store->entries.Find(propertyHandle.value)"
    "GetValue must probe the property store by the incoming handle before registry Find")
aero_require_text(
    "src/gui/core/DependencyObject.cpp"
    "Canonical-handle hot path"
    "GetValue must skip registry Find for a canonical handle that already has a store entry")
aero_require_text(
    "src/gui/styles/StyleState.hpp"
    "struct StyleSetter {\n    DependencyPropertyHandle property;"
    "Style setters must key by DependencyPropertyHandle, not property name")
aero_require_text(
    "src/gui/templates/TemplateProgram.hpp"
    "struct VisualStateSetterPlan {\n    Base::String targetName;\n    DependencyPropertyHandle property;"
    "VSM setters must key by DependencyPropertyHandle, not property name")
aero_require_text(
    "src/gui/templates/TemplateProgram.hpp"
    "struct TemplateTriggerSetter {\n    Base::String targetName;\n    DependencyPropertyHandle property;"
    "Template trigger setters must key by DependencyPropertyHandle, not property name")
aero_forbid_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "class AeroGuiInternalLayout"
    "AeroGuiInternal must remain one friend type; do not add sectional friend classes")
aero_forbid_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "AttachTemplateEngine"
    "No-op AttachTemplateEngine must not return on AeroGuiInternal")
aero_forbid_text(
    "src/gui/internal/AeroGuiInternal.hpp"
    "SetVisualStateManager"
    "No-op SetVisualStateManager must not return on AeroGuiInternal")
aero_require_text(
    "include/Aero/VisualTreeHelper.hpp"
    "class AERO_GUI_API VisualTreeHelper"
    "VisualTreeHelper must remain the public WPF visual-tree API")
aero_require_text(
    "include/Aero/LogicalTreeHelper.hpp"
    "class AERO_GUI_API LogicalTreeHelper"
    "LogicalTreeHelper must remain the public WPF logical-tree API")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "class AERO_GUI_API VisualTreeHelper"
    "VisualTreeHelper must live in VisualTreeHelper.hpp")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "class AERO_GUI_API LogicalTreeHelper"
    "LogicalTreeHelper must live in LogicalTreeHelper.hpp")
aero_forbid_text(
    "include/Aero/DependencyProperty.hpp"
    "class AERO_GUI_API DependencyObject"
    "DependencyObject must live in DependencyObject.hpp")
aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "#include <Aero/Media/DrawingContext.hpp>"
    "FrameworkElement must not hard-include DrawingContext.hpp")
aero_forbid_text(
    "include/Aero/UIElement.hpp"
    "#include <Aero/Media/DrawingContext.hpp>"
    "UIElement must not hard-include DrawingContext.hpp")
aero_require_text(
    "include/Aero/Controls/TabControl.hpp"
    "class AERO_GUI_API TabControl : public Primitives::Selector"
    "TabControl derives Selector so tabs are driven by Items/ItemsSource/SelectedIndex")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "visualChildren_"
    "Visual must not store a Vector of visual children")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "logicalChildren_"
    "Visual must not store a Vector of logical children")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "GetVisualChildren()"
    "Public GetVisualChildren Span is retired; walk Count/GetChild")
aero_require_text(
    "include/Aero/TryCast.hpp"
    "template<class T>"
    "TryCast is the TypeId-chain downcast template")
aero_require_text(
    "include/Aero/TryCast.hpp"
    "T* TryCast(Object* object) noexcept"
    "Public habit is Aero::TryCast<T>(object)")
aero_forbid_text(
    "include/Aero/TryCast.hpp"
    "#include <Aero/Meta.hpp>"
    "TryCast.hpp must not include Meta.hpp")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "AsUIElement"
    "Visual vtable must not carry AsUIElement")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "AsFrameworkElement"
    "Visual vtable must not carry AsFrameworkElement")
aero_forbid_text(
    "include/Aero/UIElement.hpp"
    "AsUIElement"
    "UIElement must not restore AsUIElement")
aero_forbid_text(
    "include/Aero/FrameworkElement.hpp"
    "AsFrameworkElement"
    "FrameworkElement must not restore AsFrameworkElement")
aero_forbid_text(
    "include/Aero/Visual.hpp"
    "static Visual* Of"
    "Visual::Of is retired; use Aero::TryCast<Media::Visual>")
aero_require_text(
    "include/Aero/Visual.hpp"
    "IsAncestorOf"
    "Visual must expose IsAncestorOf")
aero_require_text(
    "include/Aero/Visual.hpp"
    "TransformToVisual"
    "Visual must expose TransformToVisual")
aero_require_text(
    "include/Aero/Visual.hpp"
    "PointToScreen"
    "Visual must expose PointToScreen")
aero_require_text(
    "include/Aero/Visual.hpp"
    "PointFromScreen"
    "Visual must expose PointFromScreen")
aero_require_text(
    "include/Aero/Visual.hpp"
    "AddVisualChild"
    "Visual child attachment is AddVisualChild/RemoveVisualChild")
aero_require_text(
    "include/Aero/Visual.hpp"
    "OnVisualChildrenChanged"
    "Visual must expose OnVisualChildrenChanged")
aero_require_text(
    "include/Aero/Visual.hpp"
    "DependencyObject* GetLogicalParent()"
    "GetLogicalParent returns DependencyObject* so ContentElement can parent")
aero_forbid_text(
    "include/Aero/LogicalTreeHelper.hpp"
    "static Media::Visual* GetParent"
    "LogicalTreeHelper keeps DependencyObject overloads only")
aero_forbid_text(
    "include/Aero/Controls/StackPanel.hpp"
    "class AERO_GUI_API DockPanel"
    "DockPanel must live in DockPanel.hpp")
aero_forbid_text(
    "include/Aero/Controls/StackPanel.hpp"
    "class AERO_GUI_API WrapPanel"
    "WrapPanel must live in WrapPanel.hpp")
aero_forbid_text(
    "include/Aero/Controls/StackPanel.hpp"
    "class AERO_GUI_API UniformGrid"
    "UniformGrid must live in UniformGrid.hpp")
aero_forbid_text(
    "include/Aero/Controls/StackPanel.hpp"
    "class AERO_GUI_API Canvas"
    "Canvas must live in Canvas.hpp")
aero_forbid_text(
    "include/Aero/Controls/Border.hpp"
    "class AERO_GUI_API Viewbox"
    "Viewbox must live in Viewbox.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API GroupBox"
    "GroupBox must live in GroupBox.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API Label"
    "Label must live in Label.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API Expander"
    "Expander must live in Expander.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API TabItem"
    "TabItem must live in TabItem.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API TabControl"
    "TabControl must live in TabControl.hpp")
aero_forbid_text(
    "include/Aero/Controls/HeaderedContentControl.hpp"
    "class AERO_GUI_API TabPanel"
    "TabPanel must live in TabPanel.hpp")
aero_forbid_text(
    "include/Aero/FrameworkContentElement.hpp"
    "class AERO_GUI_API ContentElement"
    "ContentElement must live in ContentElement.hpp")
aero_require_text(
    "include/Aero/Visual.hpp"
    "#if defined(AERO_GUI_IMPLEMENTATION)\n    ::Aero::ElementTree* GetTree() const noexcept { return tree_; }\n#endif"
    "Visual.GetTree must not be declared on the installed SDK class")
aero_require_text(
    "include/Aero/Visual.hpp"
    "std::uint8_t renderDirtyFlags_"
    "Visual render dirty flags must remain private hot fields on Visual")
aero_require_text(
    "include/Aero/Visual.hpp"
    "std::uint32_t handleIndex_"
    "Visual handle index must remain a private hot field on Visual")
aero_require_text(
    "include/Aero/Visual.hpp"
    "Base::RenderNodeId renderNodeId_"
    "Visual render node id must remain a private hot field on Visual")
aero_require_text(
    "tools/sdk-consumers/GuiConsumer.cpp"
    "!HasVisualGetTree<Aero::Media::Visual>::value"
    "SDK consumers must prove Visual.GetTree is not part of the installed API")
aero_forbid_text(
    "include/Aero/UIElement.hpp"
    "#include <Aero/Layout.hpp>"
    "UIElement must not pull the Layout.hpp umbrella")
aero_require_text(
    "include/Aero/UIElement.hpp"
    "#include <Aero/Visibility.hpp>"
    "Visibility must live next to UIElement")
aero_require_text(
    "include/Aero/UIElement.hpp"
    "#include <Aero/Media/BlendMode.hpp>"
    "BlendMode must live in Media, not Layout.hpp")
aero_require_text(
    "include/Aero/FrameworkElement.hpp"
    "#include <Aero/HorizontalAlignment.hpp>"
    "HorizontalAlignment/VerticalAlignment/FlowDirection must live next to FrameworkElement")
aero_require_text(
    "include/Aero/Controls/Grid.hpp"
    "#include <Aero/Controls/GridLength.hpp>"
    "GridLength must be owned by the Grid header family")
aero_require_text(
    "include/Aero/Layout.hpp"
    "#include <Aero/Visibility.hpp>"
    "Layout.hpp must remain a compatibility umbrella for Visibility")
aero_require_text(
    "include/Aero/Layout.hpp"
    "#include <Aero/HorizontalAlignment.hpp>"
    "Layout.hpp must remain a compatibility umbrella for alignment enums")
aero_require_text(
    "include/Aero/Layout.hpp"
    "#include <Aero/Controls/GridLength.hpp>"
    "Layout.hpp must remain a compatibility umbrella for GridLength")
aero_require_text(
    "include/Aero/Layout.hpp"
    "#include <Aero/Media/BlendMode.hpp>"
    "Layout.hpp must remain a compatibility umbrella for BlendMode")
aero_require_text(
    "include/Aero/Layout.hpp"
    "#include <Aero/Diagnostics/Layout.hpp>"
    "Layout.hpp must remain a compatibility umbrella for LayoutDiagnostics")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "enum class Visibility"
    "Layout.hpp must not own Visibility")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "enum class HorizontalAlignment"
    "Layout.hpp must not own HorizontalAlignment")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "enum class BlendMode"
    "Layout.hpp must not own BlendMode")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "struct GridLength"
    "Layout.hpp must not own GridLength")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "struct LayoutDiagnostics"
    "Layout.hpp must not own LayoutDiagnostics")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "Threading.hpp"
    "Layout.hpp must not pull Threading.hpp")
aero_forbid_text(
    "include/Aero/Layout.hpp"
    "Media/Geometry.hpp"
    "Layout.hpp must not pull Media/Geometry.hpp")
aero_require_text(
    "include/Aero/UIElement.hpp"
    "struct LayoutHot"
    "Layout hot state must live on UIElement, not a facet bag")
aero_forbid_text(
    "include/Aero/UIElement.hpp"
    "ElementFacet"
    "Installed UIElement.hpp must not advertise element facet APIs")
aero_forbid_text(
    "include/Aero/UIElement.hpp"
    "GetFacet"
    "Installed UIElement.hpp must not advertise GetFacet")

# One public AERO_*_API class per header, with basename equal to the type
# name. Umbrella headers and remaining kitchen-sink families (split in later
# waves) are explicit exemptions. Compatibility shims with no API class skip.
set(aero_one_type_exemptions
    include/Aero/Controls.hpp
    include/Aero/Controls/Primitives.hpp
    include/Aero/Controls/RangeBase.hpp
    include/Aero/Triggers/Triggers.hpp
    include/Aero/Gui.hpp
    include/Aero/Layout.hpp
    include/Aero/Documents.hpp
    include/Aero/Events.hpp
    include/Aero/Collections.hpp
    include/Aero/Diagnostics.hpp
    include/Aero/Input.hpp
    include/Aero/Threading.hpp
    include/Aero/Meta.hpp
    include/Aero/Value.hpp
    include/Aero/Style.hpp
    include/Aero/Resources.hpp
    include/Aero/Shapes.hpp
    include/Aero/DependencyProperty.hpp
    include/Aero/VisualStateManager.hpp
    include/Aero/Media/Brushes.hpp
    include/Aero/Media/Geometry.hpp
    include/Aero/Media/Transforms.hpp
    include/Aero/Media/Effects.hpp
    include/Aero/Media/Images.hpp
    include/Aero/Media/Animation.hpp
    include/Aero/Media/Animation/MediaActions.hpp
    include/Aero/Media/Animation/StoryboardActions.hpp
    include/Aero/Controls/Panel.hpp
    include/Aero/Controls/Grid.hpp
    include/Aero/Controls/ListBox.hpp
    include/Aero/Controls/ListView.hpp
    include/Aero/Controls/ComboBox.hpp
    include/Aero/Controls/TreeView.hpp
    include/Aero/Controls/Menu.hpp
    include/Aero/Controls/ToolBar.hpp
    include/Aero/Controls/StatusBar.hpp
    include/Aero/Controls/ScrollViewer.hpp
    include/Aero/Controls/Decorator.hpp
    include/Aero/Controls/UserControl.hpp
    include/Aero/Controls/ToolTip.hpp
    include/Aero/Controls/VirtualizingStackPanel.hpp
    include/AeroApp/Application.hpp
    include/AeroApp/App.hpp
    include/AeroAudio/Audio.hpp
    include/AeroRender/Render.hpp
    include/Aero/InputInterop.hpp
    include/Aero/Base/Allocator.hpp
    include/Aero/Base/Object.hpp
    include/Aero/Triggers/TriggerBase.hpp
    include/Aero/Triggers/Conditions.hpp
    include/Aero/Media/Fonts.hpp
    include/Aero/Data/Binding.hpp
    include/Aero/Markup/XamlProvider.hpp
    include/Aero/Markup/ServiceProvider.hpp
    include/Aero/Interactivity/Behavior.hpp
    include/Aero/Interactivity/TriggerAction.hpp
    include/Aero/Interactivity/BlendBehaviors.hpp
    include/Aero/Interactivity/Conditions.hpp
    include/Aero/Interactivity/InteractionTriggers.hpp
    include/Aero/CAPI.h
    include/Aero/Module.hpp)
foreach(public_header IN LISTS AERO_PUBLIC_HEADERS)
    list(FIND aero_one_type_exemptions "${public_header}" aero_one_type_exempt_idx)
    get_filename_component(aero_header_ext "${public_header}" EXT)
    if(aero_one_type_exempt_idx EQUAL -1 AND aero_header_ext STREQUAL ".hpp")
        file(READ "${AERO_SOURCE_DIR}/${public_header}" aero_one_type_content)
        string(REGEX REPLACE
            "class[ \t]+AERO_[A-Z0-9_]*_API[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*;"
            ""
            aero_one_type_stripped
            "${aero_one_type_content}")
        string(REGEX MATCHALL
            "class[ \t]+AERO_[A-Z0-9_]*_API[ \t]+[A-Za-z_][A-Za-z0-9_]*"
            aero_one_type_matches
            "${aero_one_type_stripped}")
        if(NOT aero_one_type_matches STREQUAL "")
            set(aero_one_type_names "")
            foreach(aero_one_type_match IN LISTS aero_one_type_matches)
                string(REGEX REPLACE
                    "class[ \t]+AERO_[A-Z0-9_]*_API[ \t]+"
                    ""
                    aero_one_type_name
                    "${aero_one_type_match}")
                list(APPEND aero_one_type_names "${aero_one_type_name}")
            endforeach()
            list(REMOVE_DUPLICATES aero_one_type_names)
            list(LENGTH aero_one_type_names aero_one_type_count)
            get_filename_component(aero_header_stem "${public_header}" NAME_WE)
            if(aero_one_type_count GREATER 1)
                message(FATAL_ERROR
                    "Public header must own one AERO_*_API class (basename = type): ${public_header} declares ${aero_one_type_names}")
            endif()
            list(GET aero_one_type_names 0 aero_one_type_only)
            if(NOT aero_one_type_only STREQUAL aero_header_stem)
                message(FATAL_ERROR
                    "Public header basename must equal its AERO_*_API type: ${public_header} owns ${aero_one_type_only}")
            endif()
        endif()
    endif()
endforeach()

# Button.hpp must not transitively include full Meta, Animation, or DrawingContext.
set(aero_button_include_queue "include/Aero/Controls/Button.hpp")
set(aero_button_include_seen "")
while(aero_button_include_queue)
    list(GET aero_button_include_queue 0 aero_button_current)
    list(REMOVE_AT aero_button_include_queue 0)
    if(aero_button_current IN_LIST aero_button_include_seen)
        continue()
    endif()
    list(APPEND aero_button_include_seen "${aero_button_current}")
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${aero_button_current}")
        continue()
    endif()
    file(READ "${AERO_SOURCE_DIR}/${aero_button_current}" aero_button_content)
    string(REGEX MATCHALL
        "#[ \t]*include[ \t]+<((Aero|AeroApp|AeroRender|AeroAudio)/[^>]+)>"
        aero_button_includes
        "${aero_button_content}")
    foreach(aero_button_include IN LISTS aero_button_includes)
        string(REGEX REPLACE
            "#[ \t]*include[ \t]+<([^>]+)>"
            "include/\\1"
            aero_button_next
            "${aero_button_include}")
        list(APPEND aero_button_include_queue "${aero_button_next}")
    endforeach()
endwhile()
foreach(aero_button_forbidden IN ITEMS
        "include/Aero/Meta.hpp"
        "include/Aero/Media/Animation.hpp"
        "include/Aero/Media/DrawingContext.hpp")
    if(aero_button_forbidden IN_LIST aero_button_include_seen)
        message(FATAL_ERROR
            "Button.hpp include chain must not pull ${aero_button_forbidden}")
    endif()
endforeach()

file(GLOB_RECURSE aero_gui_kernel_files
    "${AERO_SOURCE_DIR}/src/gui/*.cpp"
    "${AERO_SOURCE_DIR}/src/gui/*.hpp"
    "${AERO_SOURCE_DIR}/src/gui/*.h"
    "${AERO_SOURCE_DIR}/src/gui/*.inl")
foreach(gui_kernel_file IN LISTS aero_gui_kernel_files)
    file(RELATIVE_PATH gui_kernel_relative
        "${AERO_SOURCE_DIR}" "${gui_kernel_file}")
    file(READ "${gui_kernel_file}" gui_kernel_content)
    foreach(retired_facet_token IN ITEMS
            "Core::Facet"
            "Core::GetFacet"
            "ElementFacet<"
            "ServiceFacets"
            "LayoutFacets"
            "ElementHost"
            "FacetTrait"
            "AttachElementFacets"
            "DetachElementFacets")
        string(FIND "${gui_kernel_content}"
            "${retired_facet_token}" retired_facet_position)
        if(NOT retired_facet_position EQUAL -1)
            message(FATAL_ERROR
                "Retired Facet/Access kernel token '${retired_facet_token}' remains in ${gui_kernel_relative}")
        endif()
    endforeach()
endforeach()

message(STATUS "Aero final architecture dependency checks passed")

if(NOT DEFINED AERO_SOURCE_DIR OR AERO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
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
        "include/Aero/Gui/Application.hpp"
        "include/Aero/Gui/Window.hpp"
        "include/Aero/Gui/DependencyObject.hpp"
        "include/Aero/Gui/DependencyProperty.hpp"
        "include/Aero/Gui/RoutedEvent.hpp"
        "include/Aero/Gui/Visual.hpp"
        "include/Aero/Gui/UIElement.hpp"
        "include/Aero/Gui/FrameworkElement.hpp"
        "include/Aero/Gui/FrameworkContentElement.hpp"
        "include/Aero/Gui/Control.hpp"
        "include/Aero/Gui/ContentControl.hpp"
        "include/Aero/Gui/Panel.hpp"
        "include/Aero/Gui/ButtonBase.hpp"
        "include/Aero/Gui/Button.hpp"
        "include/Aero/Gui/ToggleButton.hpp"
        "include/Aero/Gui/Grid.hpp"
        "include/Aero/Gui/StackPanel.hpp"
        "include/Aero/Gui/Border.hpp"
        "include/Aero/Gui/ItemsControl.hpp"
        "include/Aero/Gui/ListBox.hpp"
        "include/Aero/Gui/TreeView.hpp"
        "include/Aero/Gui/TextBlock.hpp"
        "include/Aero/Gui/TextBoxBase.hpp"
        "include/Aero/Gui/TextBox.hpp"
        "include/Aero/Gui/BindingBase.hpp"
        "include/Aero/Gui/Binding.hpp"
        "include/Aero/Gui/ResourceDictionary.hpp"
        "include/Aero/Gui/Style.hpp"
        "include/Aero/Gui/ControlTemplate.hpp"
        "include/Aero/Gui/DataTemplate.hpp"
        "include/Aero/Gui/Storyboard.hpp"
        "include/Aero/Gui/Brush.hpp"
        "include/Aero/Gui/Geometry.hpp"
        "include/Aero/Gui/Transform.hpp"
        "include/Aero/Gui/XamlReader.hpp"
        "include/Aero/Gui/View.hpp"
        "include/Aero/Gui/IRenderer.hpp"
        "include/Aero/Render/RenderDevice.hpp"
        "include/Aero/Render/RenderTarget.hpp"
        "include/Aero/Render/D3D11.hpp"
        "include/Aero/Render/OpenGL33.hpp"
        "include/Aero/Markup/ResourceScope.hpp"
        "include/Aero/Diagnostics/Rendering.hpp"
        "include/Aero/Meta.hpp"
        "include/Aero/App.hpp")
    aero_require_file("${required_public_entry}")
endforeach()

foreach(retired_public_entry IN ITEMS
        "include/Aero/Integration.hpp"
        "include/Aero/Integration"
        "include/Aero/RenderSurface.hpp"
        "include/Aero/Runtime.hpp"
        "include/Aero/RuntimeEnvironment.hpp"
        "include/Aero/RuntimeTypes.hpp"
        "include/Aero/View.hpp"
        "include/Aero/IRenderer.hpp"
        "include/Aero/RenderDevice.hpp"
        "include/Aero/RenderTarget.hpp"
        "include/Aero/Gui/RenderDevice.hpp"
        "include/Aero/Gui/RenderTarget.hpp"
        "include/Aero/Markup.hpp"
        "include/Aero/Application.hpp"
        "include/Aero/Window.hpp")
    aero_forbid_file("${retired_public_entry}")
endforeach()

aero_require_text(
    "include/Aero/Gui/IRenderer.hpp"
    "RenderTarget& target"
    "IRenderer must render to the canonical RenderTarget")
aero_forbid_text(
    "include/Aero/Gui/IRenderer.hpp"
    "RenderSurface"
    "IRenderer must not expose the retired RenderSurface spelling")
aero_require_text(
    "include/Aero/Render/RenderTarget.hpp"
    "class AERO_GUI_API RenderTarget final"
    "RenderTarget must be the installed target object")
aero_forbid_text(
    "include/Aero/Render/RenderTarget.hpp"
    "PresentMode"
    "Presentation policy must remain source-private")
aero_require_text(
    "include/Aero/Module.hpp"
    "Base::Span<const Markup::ResourceScopeRegistration> resourceScopes;"
    "Product resource scopes must cross the Gui boundary through module data")
aero_forbid_text(
    "src/markup/MarkupLoader.cpp"
    "Aero/Gui/Application.hpp"
    "AeroGui Markup must not include the AeroApp Application contract")
aero_forbid_text(
    "src/markup/MarkupLoader.cpp"
    "Aero::Application"
    "AeroGui Markup must not link back to AeroApp symbols")
aero_require_text(
    "src/app/Metadata.hpp"
    "module.resourceScopes = resourceScopes;"
    "The App module must provide its Application resource-scope capability")

foreach(backend_header IN ITEMS
        "include/Aero/Render/D3D11.hpp"
        "include/Aero/Render/OpenGL33.hpp")
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
    "include/Aero/Render/D3D11.hpp"
    "CreateD3D11RenderTarget"
    "D3D11 embedding must use RenderTarget vocabulary")
aero_require_text(
    "include/Aero/Render/OpenGL33.hpp"
    "CreateOpenGL33RenderTarget"
    "OpenGL embedding must use RenderTarget vocabulary")

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
endforeach()

foreach(required_architecture_document IN ITEMS
        "docs/ARCHITECTURE.md"
        "docs/SOURCE_ARCHITECTURE.md"
        "docs/WINDOW_HOSTING.md"
        "docs/REFACTOR_CLOSURE_S18_S24.md")
    aero_require_file("${required_architecture_document}")
endforeach()
aero_require_file("cmake/CheckWindowsExports.cmake")
aero_require_text(
    "cmake/AeroCompilerOptions.cmake"
    "function(aero_verify_windows_exports target expected_export)"
    "Shared Windows builds must validate their real DLL export tables")
aero_forbid_text(
    "tests/CMakeLists.txt"
    "Aero::Integration"
    "Framework tests must link the final Gui product rather than retired Integration")
aero_forbid_text(
    "tests/FrameworkConformanceTests.cpp"
    "Aero/Integration"
    "Framework tests must consume the current installed SDK")

file(GLOB_RECURSE aero_physical_public_headers
    RELATIVE "${AERO_SOURCE_DIR}"
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h")
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
        "src/gui/private"
        "src/controls/private"
        "src/markup/private"
        "src/media/private"
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
        "src/gui/ViewRenderer.hpp"
        "src/gui/ViewState.hpp"
        "src/markup/XamlProvider.cpp"
        "src/markup/XamlReader.cpp"
        "src/input/Clipboard.cpp"
        "src/render/RenderDevice.cpp"
        "src/render/RenderDeviceResources.cpp"
        "src/render/RenderBatch.cpp"
        "src/render/GraphicsTypes.hpp"
        "src/render/RenderTarget.cpp"
        "src/gui/ViewRendererResources.cpp"
        "src/render/RenderDeviceState.hpp"
        "src/render/RenderTargetState.hpp"
        "src/app/RenderContext.cpp")
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
    if(source_contract_name MATCHES "(Internal|Private)")
        file(RELATIVE_PATH source_contract_relative
            "${AERO_SOURCE_DIR}" "${source_contract_file}")
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
aero_require_file("src/app/Presentation.hpp")
aero_forbid_text(
    "src/app/Presentation.hpp"
    "Descriptor"
    "App presentation values must not recreate a generic native-context protocol")
aero_forbid_text(
    "src/render/RenderTargetState.hpp"
    "class NativeRenderTarget"
    "RenderTarget::Access is the only native target object")
aero_forbid_text(
    "src/render/RenderTargetState.hpp"
    "struct NativeRenderTarget"
    "RenderTarget::Access is the only native target object")
aero_forbid_text(
    "src/render/RenderDeviceState.hpp"
    "NativeRenderDevice"
    "RenderDevice::Access is the only native device object")
aero_forbid_file("src/render/private/BackendApi.hpp")
aero_forbid_text(
    "src/render/GraphicsTypes.hpp"
    "class AERO_GUI_API GraphicsDevice"
    "Command declarations must not recreate the retired generic device")
aero_forbid_text(
    "src/render/GraphicsTypes.hpp"
    "class AERO_GUI_API GraphicsBackend"
    "Native command queues must not share an abstract backend lifetime")
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
aero_forbid_text(
    "src/render/RenderDeviceState.hpp"
    "DefaultTarget("
    "RenderDevice must not own an implicit target lifetime")
aero_forbid_text(
    "src/render/RenderTargetState.hpp"
    "CreateBorrowed("
    "Every RenderTarget must own exactly one target implementation")
aero_forbid_text(
    "include/Aero/Render/RenderTarget.hpp"
    "ownsImpl_"
    "RenderTarget ownership must not be conditional")
aero_require_text(
    "src/app/OpenGL33RenderContext.cpp"
    "class OpenGLRenderContext final : public RenderContext"
    "OpenGL window presentation must belong to App::RenderContext")
aero_forbid_text(
    "src/render/d3d11/D3D11Device.cpp"
    "ViewRenderData*"
    "D3D11 RenderDevice must not own per-View renderer data")
aero_forbid_text(
    "src/render/opengl33/OpenGL33RenderDevice.hpp"
    "ViewRenderData*"
    "OpenGL RenderDevice must not own per-View renderer data")
aero_forbid_text(
    "src/render/opengl33/OpenGL33Embedded.cpp"
    "ViewRenderData*"
    "Embedded OpenGL RenderDevice must not own per-View renderer data")
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
    "src/app/RenderContext.cpp"
    "Base::Result<void> presented = PresentFrame();"
    "RenderContext must own the final desktop Present boundary")
aero_require_text(
    "src/app/RenderContext.hpp"
    "bool frameOpen_ = false;"
    "RenderContext must own desktop frame state")
foreach(retired_target_frame_state IN ITEMS
        "frameOpen" "frameRendered" "frameEnded"
        "CompletePresentation" "CancelPresentation"
        "PresentFrame" "DiscardFrame" "BeginFrame(" "EndFrame(")
    aero_forbid_text(
        "src/render/RenderTargetState.hpp"
        "${retired_target_frame_state}"
        "RenderTarget must remain a drawable target without frame state")
endforeach()
aero_require_text(
    "src/app/D3D11RenderContext.cpp"
    "class D3D11RenderContext final : public RenderContext"
    "D3D11 window/device creation must belong to a concrete RenderContext")
aero_require_text(
    "src/app/OpenGL33RenderContext.cpp"
    "class OpenGLRenderContext final : public RenderContext"
    "OpenGL window/device creation must belong to a concrete RenderContext")
aero_forbid_text(
    "src/app/RenderContext.hpp"
    "class RenderContext final"
    "RenderContext must remain the shared presentation lifecycle base")
aero_require_file("src/render/RenderBatch.hpp")
aero_forbid_file("src/render/RenderCommands.hpp")
aero_forbid_file("src/render/RenderCommands.cpp")
aero_forbid_text(
    "src/render/GraphicsTypes.hpp"
    "PipelineDescriptor"
    "Backend pipeline state must not recreate a configurable generic RHI descriptor")
aero_forbid_text(
    "src/render/GraphicsTypes.hpp"
    "ShaderDescriptor"
    "Backend shader programs must not recreate a configurable generic RHI descriptor")
aero_require_text(
    "src/render/RenderDeviceState.hpp"
    "DrawBatch("
    "Backend RenderDevice implementations must submit RenderBatch values")
aero_forbid_text(
    "src/render/d3d11/D3D11RenderDeviceDraw1.inc"
    "DrawPhase"
    "D3D11 must execute one RenderBatch directly without an internal command model")
aero_forbid_text(
    "src/render/opengl33/OpenGL33RenderDevice.cpp"
    "DrawPhase"
    "OpenGL must execute one RenderBatch directly without an internal command model")
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
    "src/app/D3D11RenderContext.cpp"
    "CreateD3D11WindowSurface"
    "Desktop D3D11 hosting must explicitly compose device and target")
aero_forbid_text(
    "src/app/OpenGL33RenderContext.cpp"
    "CreateOpenGL33WindowSurface"
    "Desktop OpenGL hosting must explicitly compose device and target")

# ---------------------------------------------------------------------------
# WPF code-behind experience
# ---------------------------------------------------------------------------
aero_require_text(
    "include/Aero/Gui/Window.hpp"
    "void InitializeComponent() noexcept;"
    "Window code-behind must expose the conventional InitializeComponent entry")
aero_require_text(
    "include/Aero/Meta.hpp"
    "TypeDescription& EventHandler("
    "Component metadata must expose a direct XAML event-handler description")
aero_require_text(
    "include/Aero/Meta.hpp"
    "DefineComponentModule("
    "Custom components must not require handwritten Registry or facet adapters")
aero_require_text(
    "src/markup/MarkupWriter.cpp"
    "ObjectBuilder::ConnectEvent("
    "XAML event attributes must connect through the object-writer pipeline")
aero_require_file("templates/AeroApp/App.xaml")
aero_require_file("templates/AeroApp/MainWindow.xaml")

# ---------------------------------------------------------------------------
# Gui / XAML / View ownership
# ---------------------------------------------------------------------------
aero_require_text(
    "include/Aero/Gui/XamlReader.hpp"
    "explicit XamlReader(Aero::Gui& gui)"
    "XamlReader must be Gui-owned rather than View-owned")
aero_forbid_text(
    "include/Aero/Gui/XamlReader.hpp"
    "XamlReader(Aero::View&"
    "XamlReader must not recreate View-owned loading")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "friend class Markup::ReloadCoordinator"
    "ReloadCoordinator must not require View private access")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "QueryReloadSource("
    "Reload source/cache ownership belongs to Gui/Markup")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "SetViewport("
    "Installed View must expose SetSize and SetScale rather than a generic viewport protocol")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "DispatchPointer("
    "Installed View must expose host-friendly pointer methods")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "DispatchKeyboard("
    "Installed View must expose host-friendly keyboard methods")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "DispatchText("
    "Installed View must expose Char rather than a generic text dispatch protocol")
foreach(view_private_operation IN ITEMS
        "MountContent(" "UnmountContent(" "LoadResources("
        "LoadCompiledResources(" "SetResourceDictionary("
        "LoadBuiltInTheme(" "ExecuteFrame(" "AdvanceClocks("
        "AdvanceAnimations(" "FindNamedObject(" "NamedObjectCount("
        "IsInstanceOf(")
    aero_forbid_text(
        "include/Aero/Gui/View.hpp"
        "${view_private_operation}"
        "View implementation operations must remain source-only")
endforeach()
aero_require_text(
    "src/markup/ReloadCoordinator.cpp"
    "gui->xaml.QuerySource"
    "ReloadCoordinator must query the Gui-owned XAML runtime directly")
aero_require_file("src/gui/ViewRenderer.hpp")
aero_forbid_file("cmake/AeroRuntimeTargets.cmake")
aero_forbid_file("cmake/AeroGuiRuntimeTargets.cmake")
aero_forbid_file("cmake/AeroGuiCompositionTargets.cmake")
aero_forbid_file("cmake/AeroRenderingTargets.cmake")
aero_forbid_text(
    "include/Aero/Gui/View.hpp"
    "Runtime::Detail"
    "View must not expose a generic Runtime implementation namespace")
aero_forbid_text(
    "src/media/ImageCache.hpp"
    "Runtime::Detail"
    "ImageCache belongs to Media rather than a generic Runtime namespace")
aero_forbid_text(
    "src/text/TextPipeline.hpp"
    "Runtime::Detail"
    "TextPipeline belongs to Text rather than a generic Runtime namespace")
aero_forbid_text(
    "src/text/TextPipeline.cpp"
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
    "src/controls/DataTemplateTriggerState.hpp"
    "Aero::Runtime::Detail"
    "DataTemplate trigger state belongs to Controls")
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
aero_forbid_file("src/controls/ControlsPrivate.hpp")
aero_forbid_file("src/markup/MarkupPrivate.hpp")
aero_forbid_file("src/media/MediaPrivate.hpp")
aero_require_text(
    "include/Aero/Gui/Button.hpp"
    "class AERO_GUI_API Button"
    "Button.hpp must own the Button declaration")
aero_forbid_file("include/Aero/Gui/Primitives.hpp")

# S14: public headers are organized by WPF-visible type ownership. The old
# implementation-category family headers remain include-only compatibility
# umbrellas and must never regain declarations.
foreach(s14_owner IN ITEMS
        "include/Aero/Gui/Control.hpp|class AERO_GUI_API Control"
        "include/Aero/Gui/ContentControl.hpp|class AERO_GUI_API ContentControl"
        "include/Aero/Gui/Panel.hpp|class AERO_GUI_API Panel"
        "include/Aero/Gui/Grid.hpp|class AERO_GUI_API Grid"
        "include/Aero/Gui/ListBox.hpp|class AERO_GUI_API ListBox"
        "include/Aero/Gui/ComboBox.hpp|class AERO_GUI_API ComboBox"
        "include/Aero/Gui/ListView.hpp|class AERO_GUI_API ListView"
        "include/Aero/Gui/TreeView.hpp|class AERO_GUI_API TreeView"
        "include/Aero/Gui/TextBox.hpp|class AERO_GUI_API TextBox")
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
foreach(s14_umbrella IN ITEMS
        "include/Aero/Gui/Text.hpp")
    aero_forbid_text(
        "${s14_umbrella}"
        "class AERO_GUI_API"
        "S14 family compatibility headers must be include-only")
    aero_forbid_text(
        "${s14_umbrella}"
        "enum class"
        "S14 family compatibility headers must not own enums")
endforeach()
aero_forbid_text(
    "include/Aero/Value.hpp"
    "struct Members"
    "WPF-facing dependency properties and routed events must not use the retired Members category")
# ---------------------------------------------------------------------------
# Public API and build model
# ---------------------------------------------------------------------------
aero_forbid_text(
    "include/Aero/Gui/Application.hpp"
    "RunChecked"
    "Application must expose one Result-returning Run family")
aero_forbid_text(
    "include/Aero/Gui/Application.hpp"
    "Run(Base::Ref<Window>"
    "Explicit windows must be supplied through SetMainWindow")
aero_forbid_text(
    "include/Aero/Gui/Application.hpp"
    "SetMainWindowBorrowed"
    "Application ownership adapters must remain source-only")
aero_forbid_text(
    "include/Aero/Gui/ItemsControl.hpp"
    "SetItemsSourceBorrowed"
    "ItemsControl ownership adapters must remain source-only")
aero_forbid_text(
    "include/Aero/Gui/Window.hpp"
    "ShowChecked"
    "Window must expose one Result-returning Show API")
aero_forbid_text(
    "include/Aero/Gui/Window.hpp"
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
        "add_library(Aero::Gui ALIAS AeroGui)"
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
if(product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*AeroRender[ \t\r\n)]" OR
   product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*Aero::Render[ \t\r\n]")
    message(FATAL_ERROR
        "Render remains a specialist namespace in the single AeroGui product; do not add a thin AeroRender product layer")
endif()
aero_require_text(
    "cmake/AeroGuiTargets.cmake"
    "target_sources(AeroGui PRIVATE"
    "Rendering implementation must remain inside the single embeddable AeroGui product")
foreach(retired_object_layer IN ITEMS
        AeroGuiKernelObjects AeroControlsObjects AeroMarkupKernelObjects
        AeroMarkupObjects AeroInspectorObjects AeroRuntimeObjects
        AeroRenderingObjects)
    if(product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*${retired_object_layer}")
        message(FATAL_ERROR
            "Product implementation object layer was recreated: ${retired_object_layer}")
    endif()
endforeach()

message(STATUS "Aero final architecture dependency checks passed")

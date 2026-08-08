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
        "include/Aero/View.hpp"
        "include/Aero/IRenderer.hpp"
        "include/Aero/RenderDevice.hpp"
        "include/Aero/RenderTarget.hpp"
        "include/Aero/Render/D3D11.hpp"
        "include/Aero/Render/OpenGL33.hpp"
        "include/Aero/Diagnostics/Rendering.hpp"
        "include/Aero/Markup.hpp"
        "include/Aero/Meta.hpp"
        "include/Aero/App.hpp"
        "include/Aero/Application.hpp"
        "include/Aero/Window.hpp")
    aero_require_file("${required_public_entry}")
endforeach()

foreach(retired_public_entry IN ITEMS
        "include/Aero/Integration.hpp"
        "include/Aero/Integration"
        "include/Aero/RenderSurface.hpp"
        "include/Aero/Runtime.hpp"
        "include/Aero/RuntimeEnvironment.hpp"
        "include/Aero/RuntimeTypes.hpp")
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
    "include/Aero/RenderTarget.hpp"
    "class AERO_API RenderTarget final"
    "RenderTarget must be the installed target object")
aero_forbid_text(
    "include/Aero/RenderTarget.hpp"
    "PresentMode"
    "Presentation policy must remain source-private")

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
endforeach()

# ---------------------------------------------------------------------------
# Source ownership
# ---------------------------------------------------------------------------
foreach(retired_source_entry IN ITEMS
        "src/integration"
        "src/runtime"
        "src/providers"
        "src/platform"
        "src/render/DeviceRenderer.cpp"
        "src/render/DeviceRenderer.hpp"
        "src/render/RenderPrivate.hpp"
        "src/render/RenderSurface.cpp"
        "src/render/private/RenderSurface.hpp")
    aero_forbid_file("${retired_source_entry}")
endforeach()

foreach(required_source_entry IN ITEMS
        "src/gui/Gui.cpp"
        "src/gui/View.cpp"
        "src/markup/XamlProvider.cpp"
        "src/markup/XamlReader.cpp"
        "src/input/Clipboard.cpp"
        "src/render/RenderDevice.cpp"
        "src/render/RenderTarget.cpp"
        "src/render/Renderer.cpp"
        "src/render/private/RenderDevice.hpp"
        "src/render/private/RenderTarget.hpp"
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
endforeach()
unset(aero_source_contract_files)
unset(source_contract_content)
unset(source_contract_relative)
unset(runtime_detail_position)
unset(runtime_namespace_position)

# ---------------------------------------------------------------------------
# Rendering ownership
# ---------------------------------------------------------------------------
aero_require_text(
    "src/render/Renderer.hpp"
    "RenderOnscreen("
    "Renderer must own onscreen command recording and submission")
aero_require_text(
    "src/render/Renderer.hpp"
    "RenderOffscreen("
    "Renderer must own offscreen command recording and submission")
aero_forbid_text(
    "src/render/Renderer.hpp"
    "SurfaceSession"
    "Renderer must not depend on a second surface lifecycle")
aero_forbid_text(
    "src/render/Surface.hpp"
    "class AERO_API SurfaceSession"
    "SurfaceSession lifecycle must not be recreated")
aero_forbid_text(
    "src/render/private/RenderTarget.hpp"
    "class NativeRenderTarget"
    "RenderTarget::Impl is the only native target object")
aero_forbid_text(
    "src/render/private/RenderTarget.hpp"
    "struct NativeRenderTarget"
    "RenderTarget::Impl is the only native target object")
aero_forbid_text(
    "src/render/private/RenderDevice.hpp"
    "NativeRenderDevice"
    "RenderDevice::Impl is the only native device object")
aero_forbid_text(
    "src/render/private/BackendApi.hpp"
    "RenderSurface"
    "Private backend factories must use RenderTarget vocabulary")
aero_forbid_text(
    "src/render/private/BackendApi.hpp"
    "EmbeddedSurfaceOptions"
    "Embedded target factories must not recreate Surface vocabulary")
aero_forbid_text(
    "src/render/private/BackendApi.hpp"
    "WindowSurfaceOptions"
    "Window target factories must not recreate Surface vocabulary")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "using FrameEncoder ="
    "The low-level command encoder must not be exposed through a migration alias")
aero_require_text(
    "src/render/FrameEncoder.hpp"
    "class CommandEncoder"
    "The low-level recorder must be named CommandEncoder")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "class Renderer {"
    "FrameEncoder must not recreate a peer semantic Renderer")
aero_forbid_text(
    "src/render/FrameEncoder.hpp"
    "using RenderTarget ="
    "The frame attachment value must not shadow the SDK RenderTarget")
aero_forbid_text(
    "src/render/private/RenderDevice.hpp"
    "DefaultTarget("
    "RenderDevice must not own an implicit target lifetime")
aero_forbid_text(
    "src/render/private/RenderTarget.hpp"
    "CreateBorrowed("
    "Every RenderTarget must own exactly one target implementation")
aero_forbid_text(
    "include/Aero/RenderTarget.hpp"
    "ownsImpl_"
    "RenderTarget ownership must not be conditional")
aero_require_text(
    "src/render/opengl33/OpenGL33Device.cpp"
    "class OpenGL33WindowTargetState final"
    "OpenGL window presentation must have an explicit RenderTarget implementation")
aero_require_text(
    "src/render/opengl33/OpenGL33Device.cpp"
    "std::uint64_t nextFrameSerial_ = 1U;"
    "OpenGL window frame serial ownership must belong to the target")
aero_forbid_text(
    "src/render/Surface.hpp"
    "EglWindow"
    "Speculative EGL surface vocabulary is outside the current product")
aero_forbid_text(
    "src/render/Surface.hpp"
    "WebGL2Canvas"
    "Speculative WebGL surface vocabulary is outside the current product")
aero_forbid_text(
    "src/render/FrameEncoder.cpp"
    "using Renderer ="
    "CommandEncoder must not retain a local Renderer migration alias")
aero_forbid_text(
    "src/render/FrameEncoder.cpp"
    "using RenderTarget ="
    "CommandEncoder must use FrameTarget directly")
aero_forbid_text(
    "src/render/opengl33/OpenGL33Device.cpp"
    "Graphics::ISurfaceBackend"
    "OpenGL must use the canonical WindowSurfaceBackend contract")
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
    "renderer.Render(*target_)"
    "RenderContext must own the final desktop target handoff")
aero_forbid_text(
    "src/app/RenderContext.cpp"
    "CreateD3D11WindowSurface"
    "Desktop D3D11 hosting must explicitly compose device and target")
aero_forbid_text(
    "src/app/RenderContext.cpp"
    "CreateOpenGL33WindowSurface"
    "Desktop OpenGL hosting must explicitly compose device and target")

# ---------------------------------------------------------------------------
# Gui / XAML / View ownership
# ---------------------------------------------------------------------------
aero_require_text(
    "include/Aero/Markup.hpp"
    "explicit XamlReader(Aero::Gui& gui)"
    "XamlReader must be Gui-owned rather than View-owned")
aero_forbid_text(
    "include/Aero/Markup.hpp"
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
    "src/markup/ReloadCoordinator.cpp"
    "state.xaml.QuerySource"
    "ReloadCoordinator must query the Gui-owned XAML runtime directly")
aero_forbid_file("src/render/ViewRenderer.hpp")
aero_forbid_file("cmake/AeroRuntimeTargets.cmake")
aero_forbid_file("cmake/AeroGuiRuntimeTargets.cmake")
aero_require_file("cmake/AeroGuiCompositionTargets.cmake")
aero_forbid_text(
    "include/Aero/View.hpp"
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
    "cmake/AeroGuiCompositionTargets.cmake"
    "AERO_INTERNAL_RUNTIME"
    "Gui composition must not recreate the generic Runtime product spelling")
aero_forbid_text(
    "cmake/AeroGuiCompositionTargets.cmake"
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
    "DataTemplate trigger state belongs to Controls::Detail")
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
# Compatibility private aggregators remain source-only while audited high-fan-out
# headers use explicit narrow contracts. Do not break unrelated translation units
# merely to enforce include topology by string matching.
aero_require_text(
    "include/Aero/Controls/Button.hpp"
    "class AERO_API Button"
    "Button.hpp must own the Button declaration")
aero_forbid_text(
    "include/Aero/Controls/Primitives.hpp"
    "class AERO_API Button :"
    "Button must not have two declaration owners")

# ---------------------------------------------------------------------------
# Public API and build model
# ---------------------------------------------------------------------------
aero_forbid_text(
    "include/Aero/Application.hpp"
    "RunChecked"
    "Application must expose one Result-returning Run family")
aero_forbid_text(
    "include/Aero/Application.hpp"
    "Run(Base::Ref<Window>"
    "Explicit windows must be supplied through SetMainWindow")
aero_forbid_text(
    "include/Aero/Window.hpp"
    "ShowChecked"
    "Window must expose one Result-returning Show API")
aero_forbid_text(
    "include/Aero/Window.hpp"
    "CloseChecked"
    "Window must expose one Result-returning Close API")

aero_require_text(
    "cmake/AeroRenderingTargets.cmake"
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
    "cmake/AeroRenderingTargets.cmake"
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

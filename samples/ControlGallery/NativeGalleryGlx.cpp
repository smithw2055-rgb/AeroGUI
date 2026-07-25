#include "GalleryRuntime.hpp"

#include <Aero/Platform/X11Window.hpp>
#include <Aero/Rhi/GlxSurface.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Platform;
using namespace Rhi;
using namespace Render;

constexpr std::uint32_t GalleryWidth = 900U;
constexpr std::uint32_t GalleryHeight = 640U;

NativeSurfaceDescriptor MakeDescriptor() noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::GlxWindow;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = GalleryWidth;
    descriptor.height = GalleryHeight;
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.glx.screen = -1;
    descriptor.stableId = UINT64_C(0x4347474C583333);
    return descriptor;
}

bool RequestsClose(
    WindowEventType type) noexcept {
    return type == WindowEventType::CloseRequested ||
        type == WindowEventType::Closed;
}

Result<void> SubmitOpenGlFrame(
    OpenGL33RenderPlanBackend& renderer,
    OpenGL33GraphicsBackend& backend,
    const GalleryRuntime& runtime) noexcept {
    Result<void> submitted = renderer.Submit(
        runtime.Plan());
    if (!submitted) {
        return submitted.GetStatus();
    }
    return backend.WaitForFence(
        renderer.LastSubmittedFence());
}

Result<void> RunWindowLoop(
    X11Window& window,
    GalleryRuntime& runtime,
    SurfaceSession& surface,
    OpenGL33RenderPlanBackend& renderer,
    OpenGL33GraphicsBackend& backend) noexcept {
    while (window.IsOpen()) {
        WindowEvent event;
        Result<bool> received = window.WaitEvent(event);
        if (!received) {
            return received.GetStatus();
        }
        if (!received.Value() || RequestsClose(event.type)) {
            break;
        }

        Result<bool> runtimeFrame =
            runtime.HandleWindowEvent(event);
        if (!runtimeFrame) {
            return runtimeFrame.GetStatus();
        }
        if (event.type == WindowEventType::Resized ||
            event.type == WindowEventType::ScaleChanged) {
            if (event.width == 0U || event.height == 0U) {
                continue;
            }
            Result<void> resized = surface.Resize(
                event.width, event.height);
            if (!resized) {
                return resized.GetStatus();
            }
        }
        if (runtimeFrame.Value()) {
            Result<void> rendered = SubmitOpenGlFrame(
                renderer, backend, runtime);
            if (!rendered) {
                return rendered.GetStatus();
            }
        }
    }
    return {};
}

Result<void> RunOpenGlSession(
    SurfaceSession& surface,
    GlxSurfaceBackend& native,
    GalleryRuntime& runtime,
    X11Window* interactiveWindow) noexcept {
    Result<GlFunctionTable> functions =
        native.LoadFunctions();
    if (!functions) {
        return functions.GetStatus();
    }
    Result<GlContextContract> contract =
        native.ContextContract();
    if (!contract) {
        return contract.GetStatus();
    }

    OpenGL33GraphicsBackend backend(
        functions.Value(), contract.Value());
    Result<void> result = backend.Initialize();
    if (!result) {
        return result.GetStatus();
    }
    {
        RhiDevice device(backend);
        result = device.Initialize();
        if (result) {
            OpenGL33RenderPlanBackend renderer(
                device,
                backend,
                surface,
                contract.Value().generation);
            result = renderer.Initialize();
            if (result) {
                result = SubmitOpenGlFrame(
                    renderer, backend, runtime);
            }
            if (result && interactiveWindow != nullptr) {
                result = RunWindowLoop(
                    *interactiveWindow,
                    runtime,
                    surface,
                    renderer,
                    backend);
            }
            renderer.Shutdown();
            if (result) {
                Result<std::uint32_t> collected =
                    device.CollectGarbage();
                if (!collected) {
                    result = collected.GetStatus();
                }
            }
        }
    }
    backend.Shutdown();
    return result;
}

Result<void> AttachInteractiveWindow(
    X11Window& window,
    GlxSurfaceBackend& native) noexcept {
    return window.Attach(
        native.NativeDisplay(),
        native.NativeDrawable(),
        GalleryWidth,
        GalleryHeight,
        "AeroGUI ControlGallery",
        true);
}

} // namespace

Base::Result<void> RunControlGalleryGlx(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    GlxSurfaceBackend native;
    SurfaceSession surface(native);
    NativeSurfaceDescriptor descriptor =
        MakeDescriptor();
    Result<void> status = surface.Initialize(descriptor);
    if (!status) {
        return status.GetStatus();
    }

    X11Window window;
    if (simulateContextLoss) {
        status = RunOpenGlSession(
            surface, native, runtime, nullptr);
        if (status) {
            status = surface.NotifyContextLost();
        }
        if (status) {
            status = surface.Restore(descriptor);
        }
        if (status && interactive) {
            status = AttachInteractiveWindow(
                window, native);
        }
        if (status) {
            status = RunOpenGlSession(
                surface,
                native,
                runtime,
                interactive ? &window : nullptr);
        }
    } else {
        if (interactive) {
            status = AttachInteractiveWindow(
                window, native);
        }
        if (status) {
            status = RunOpenGlSession(
                surface,
                native,
                runtime,
                interactive ? &window : nullptr);
        }
    }
    window.Close();
    surface.Shutdown();
    return status;
}

} // namespace Aero::Samples::ControlGallery

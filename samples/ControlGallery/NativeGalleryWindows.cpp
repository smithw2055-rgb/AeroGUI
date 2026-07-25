#include "GalleryRuntime.hpp"

#include <Aero/Platform/Win32Window.hpp>
#include <Aero/Rhi/D3D11Backend.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Rhi/WglSurface.hpp>
#include <Aero/Render/D3D11RendererBackend.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Platform;
using namespace Rhi;
using namespace Render;

constexpr std::uint32_t GalleryWidth = 900U;
constexpr std::uint32_t GalleryHeight = 640U;

WindowDescriptor MakeWindowDescriptor(
    bool visible) noexcept {
    WindowDescriptor descriptor;
    descriptor.title = "AeroGUI ControlGallery";
    descriptor.width = GalleryWidth;
    descriptor.height = GalleryHeight;
    descriptor.visible = visible;
    descriptor.resizable = true;
    return descriptor;
}

NativeSurfaceDescriptor MakeD3D11Descriptor(
    D3D11GraphicsBackend& backend,
    const Win32Window& window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::D3D11Window;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = window.ClientWidth();
    descriptor.height = window.ClientHeight();
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId = UINT64_C(0x43474433443131);
    descriptor.d3d11.window = window.NativeHandle().window;
    descriptor.d3d11.device = backend.NativeDevice();
    descriptor.d3d11.immediateContext =
        backend.NativeImmediateContext();
    return descriptor;
}

NativeSurfaceDescriptor MakeWglDescriptor(
    const Win32Window& window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::WglWindow;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = window.ClientWidth();
    descriptor.height = window.ClientHeight();
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId = UINT64_C(0x434757474C3333);
    descriptor.wgl.window = window.NativeHandle().window;
    return descriptor;
}

bool RequestsClose(
    WindowEventType type) noexcept {
    return type == WindowEventType::CloseRequested ||
        type == WindowEventType::Closed;
}

Result<void> SubmitD3D11Frame(
    D3D11RenderPlanBackend& renderer,
    D3D11GraphicsBackend& backend,
    const GalleryRuntime& runtime) noexcept {
    Result<void> submitted = renderer.Submit(
        runtime.Plan());
    if (!submitted) {
        return submitted.GetStatus();
    }
    return backend.WaitForFence(
        renderer.LastSubmittedFence());
}

Result<void> RunD3D11WindowLoop(
    Win32Window& window,
    GalleryRuntime& runtime,
    D3D11SurfacePresenter& presenter,
    D3D11RenderPlanBackend& renderer,
    D3D11GraphicsBackend& backend) noexcept {
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
            Result<void> resized = presenter.Resize(
                event.width, event.height);
            if (!resized) {
                return resized.GetStatus();
            }
        }
        if (runtimeFrame.Value()) {
            Result<void> rendered = SubmitD3D11Frame(
                renderer, backend, runtime);
            if (!rendered) {
                return rendered.GetStatus();
            }
        }
    }
    return {};
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

Result<void> RunOpenGlWindowLoop(
    Win32Window& window,
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
    WglSurfaceBackend& native,
    GalleryRuntime& runtime,
    Win32Window* interactiveWindow) noexcept {
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
                result = RunOpenGlWindowLoop(
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

} // namespace

Base::Result<void> RunControlGalleryD3D11(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) {
        return status.GetStatus();
    }

    D3D11BackendOptions options;
    options.deviceMode = interactive
        ? D3D11DeviceMode::Hardware
        : D3D11DeviceMode::Warp;
    options.allowWarpFallback = true;
    D3D11GraphicsBackend backend(options);
    status = backend.Initialize();
    if (!status) {
        return status.GetStatus();
    }
    {
        RhiDevice device(backend);
        status = device.Initialize();
        if (!status) {
            backend.Shutdown();
            return status.GetStatus();
        }
        D3D11SwapChainSurface native(backend);
        SurfaceSession surface(native);
        NativeSurfaceDescriptor descriptor =
            MakeD3D11Descriptor(backend, window);
        status = surface.Initialize(descriptor);
        if (!status) {
            backend.Shutdown();
            return status.GetStatus();
        }
        D3D11SurfacePresenter presenter(
            device, backend, surface);
        status = presenter.Initialize();
        D3D11RenderPlanBackend renderer(
            device, presenter);
        if (status) {
            status = renderer.Initialize();
        }
        if (status) {
            status = SubmitD3D11Frame(
                renderer, backend, runtime);
        }
        if (status && simulateContextLoss) {
            renderer.Shutdown();
            presenter.Shutdown();
            status = surface.NotifyContextLost();
            if (status) {
                status = surface.Restore(descriptor);
            }
            if (status) {
                status = presenter.Initialize();
            }
            if (status) {
                status = renderer.Initialize();
            }
            if (status) {
                status = SubmitD3D11Frame(
                    renderer, backend, runtime);
            }
        }
        if (status && interactive) {
            status = RunD3D11WindowLoop(
                window,
                runtime,
                presenter,
                renderer,
                backend);
        }
        renderer.Shutdown();
        presenter.Shutdown();
        surface.Shutdown();
    }
    backend.Shutdown();
    return status;
}

Base::Result<void> RunControlGalleryWgl(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) {
        return status.GetStatus();
    }

    WglSurfaceBackend native;
    SurfaceSession surface(native);
    NativeSurfaceDescriptor descriptor =
        MakeWglDescriptor(window);
    status = surface.Initialize(descriptor);
    if (!status) {
        return status.GetStatus();
    }

    if (simulateContextLoss) {
        status = RunOpenGlSession(
            surface, native, runtime, nullptr);
        if (status) {
            status = surface.NotifyContextLost();
        }
        if (status) {
            status = surface.Restore(descriptor);
        }
        if (status) {
            status = RunOpenGlSession(
                surface,
                native,
                runtime,
                interactive ? &window : nullptr);
        }
    } else {
        status = RunOpenGlSession(
            surface,
            native,
            runtime,
            interactive ? &window : nullptr);
    }
    surface.Shutdown();
    return status;
}

} // namespace Aero::Samples::ControlGallery

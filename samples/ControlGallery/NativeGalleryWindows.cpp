#include "GalleryRuntime.hpp"

#include <Aero/Integration/D3D11.hpp>
#include <Aero/Integration/OpenGL33.hpp>
#include <Aero/Platform/Win32Window.hpp>

#include <utility>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Integration;
using namespace Platform;

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

bool RequestsClose(
    WindowEventType type) noexcept {
    return type == WindowEventType::CloseRequested ||
        type == WindowEventType::Closed;
}

Result<void> RunWindowLoop(
    Win32Window& window,
    GalleryRuntime& runtime,
    RenderEndpoint& endpoint) noexcept {
    while (window.IsOpen()) {
        WindowEvent event;
        Result<bool> received = window.WaitEvent(event);
        if (!received) return received.GetStatus();
        if (!received.Value() ||
            RequestsClose(event.type)) {
            break;
        }

        Result<bool> handled =
            runtime.HandleWindowEvent(event);
        if (!handled) return handled.GetStatus();
        if ((event.type == WindowEventType::Resized ||
             event.type == WindowEventType::ScaleChanged) &&
            event.width != 0U && event.height != 0U) {
            Result<void> resized =
                endpoint.Resize(
                    event.width, event.height);
            if (!resized) return resized.GetStatus();
        }
    }
    return endpoint.WaitIdle();
}

Result<void> ExerciseLoss(
    GalleryRuntime& runtime,
    RenderEndpoint& endpoint,
    bool deviceLoss) noexcept {
    Result<void> status = deviceLoss
        ? endpoint.NotifyDeviceLost()
        : endpoint.NotifySurfaceLost();
    if (status) status = endpoint.Restore();
    if (!status) return status.GetStatus();
    WindowEvent refresh;
    refresh.type = WindowEventType::Exposed;
    Result<bool> refreshed =
        runtime.HandleWindowEvent(refresh);
    return refreshed
        ? Result<void>()
        : Result<void>(refreshed.GetStatus());
}

} // namespace

Base::Result<void> RunControlGalleryD3D11(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) return status.GetStatus();

    D3D11WindowEndpointOptions options;
    options.window = window.NativeHandle();
    options.width = window.ClientWidth();
    options.height = window.ClientHeight();
    options.presentMode = RenderPresentMode::Immediate;
    options.useWarp = !interactive;
    options.allowWarpFallback = true;
    Result<Ref<RenderEndpoint>> created =
        CreateD3D11WindowEndpoint(options);
    if (!created) return created.GetStatus();
    Ref<RenderEndpoint> endpoint =
        std::move(created).Value();

    status = runtime.UseRenderEndpoint(endpoint);
    if (status && simulateContextLoss) {
        status = ExerciseLoss(
            runtime, *endpoint, true);
    }
    if (status && interactive) {
        status = RunWindowLoop(
            window, runtime, *endpoint);
    }
    if (status) status = endpoint->WaitIdle();
    if (status) {
        status = runtime.ReleaseRenderEndpoint();
    }
    return status;
}

Base::Result<void> RunControlGalleryWgl(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Win32Window window;
    Result<void> status = window.Create(
        MakeWindowDescriptor(interactive));
    if (!status) return status.GetStatus();

    OpenGL33WindowEndpointOptions options;
    options.window = window.NativeHandle();
    options.width = window.ClientWidth();
    options.height = window.ClientHeight();
    options.presentMode = RenderPresentMode::Immediate;
    Result<Ref<RenderEndpoint>> created =
        CreateOpenGL33WindowEndpoint(options);
    if (!created) return created.GetStatus();
    Ref<RenderEndpoint> endpoint =
        std::move(created).Value();

    status = runtime.UseRenderEndpoint(endpoint);
    if (status && simulateContextLoss) {
        status = ExerciseLoss(
            runtime, *endpoint, false);
    }
    if (status && interactive) {
        status = RunWindowLoop(
            window, runtime, *endpoint);
    }
    if (status) status = endpoint->WaitIdle();
    if (status) {
        status = runtime.ReleaseRenderEndpoint();
    }
    return status;
}

} // namespace Aero::Samples::ControlGallery

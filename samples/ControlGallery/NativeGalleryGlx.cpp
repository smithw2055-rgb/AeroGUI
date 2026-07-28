#include "GalleryRuntime.hpp"

#include <Aero/Integration/OpenGL33.hpp>
#include <Aero/Platform/X11Window.hpp>

#include <utility>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Integration;
using namespace Platform;

constexpr std::uint32_t GalleryWidth = 900U;
constexpr std::uint32_t GalleryHeight = 640U;

bool RequestsClose(
    WindowEventType type) noexcept {
    return type == WindowEventType::CloseRequested ||
        type == WindowEventType::Closed;
}

Result<void> RunWindowLoop(
    X11Window& window,
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

} // namespace

Base::Result<void> RunControlGalleryGlx(
    GalleryRuntime& runtime,
    bool simulateContextLoss,
    bool interactive) noexcept {
    X11Window window;
    WindowDescriptor descriptor;
    descriptor.title = "AeroGUI ControlGallery";
    descriptor.width = GalleryWidth;
    descriptor.height = GalleryHeight;
    descriptor.visible = interactive;
    Result<void> status = window.Create(descriptor);
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
        status = endpoint->NotifySurfaceLost();
        if (status) status = endpoint->Restore();
        if (status) {
            WindowEvent refresh;
            refresh.type = WindowEventType::Exposed;
            Result<bool> refreshed =
                runtime.HandleWindowEvent(refresh);
            if (!refreshed) {
                status = refreshed.GetStatus();
            }
        }
    }
    if (status && interactive) {
        status = RunWindowLoop(
            window, runtime, *endpoint);
    }
    if (status) status = endpoint->WaitIdle();
    if (status) {
        status = runtime.ReleaseRenderEndpoint();
    }
    window.Close();
    return status;
}

} // namespace Aero::Samples::ControlGallery

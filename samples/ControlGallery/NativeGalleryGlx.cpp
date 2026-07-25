#include "GalleryRuntime.hpp"

#include <Aero/Rhi/GlxSurface.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Rhi;
using namespace Render;

NativeSurfaceDescriptor
MakeDescriptor() noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind =
        SurfaceKind::GlxWindow;
    descriptor.ownership =
        SurfaceOwnership::Owned;
    descriptor.presentMode =
        PresentMode::Immediate;
    descriptor.width = 900U;
    descriptor.height = 640U;
    descriptor.colorFormat =
        GraphicsTextureFormat::Bgra8Unorm;
    descriptor.glx.screen = -1;
    descriptor.stableId =
        UINT64_C(0x4347474C583333);
    return descriptor;
}

Result<void> SubmitOpenGl(
    SurfaceSession& surface,
    GlxSurfaceBackend& native,
    const Presentation::RenderPlan&
        plan) noexcept {
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
        functions.Value(),
        contract.Value());
    Result<void> initialized =
        backend.Initialize();
    if (!initialized) {
        return initialized.GetStatus();
    }
    Result<void> result;
    {
        RhiDevice device(backend);
        result = device.Initialize();
        if (result) {
            OpenGL33RenderPlanBackend
                renderer(
                    device,
                    backend,
                    surface,
                    contract.Value().
                        generation);
            result = renderer.Initialize();
            if (result) {
                result =
                    renderer.Submit(plan);
            }
            if (result) {
                result =
                    backend.WaitForFence(
                        renderer.
                            LastSubmittedFence());
            }
            renderer.Shutdown();
            if (result) {
                Result<std::uint32_t>
                    collected =
                        device.CollectGarbage();
                if (!collected) {
                    result =
                        collected.GetStatus();
                }
            }
        }
    }
    backend.Shutdown();
    return result;
}

void RunMessageLoop(
    GlxSurfaceBackend& native) noexcept {
    auto* display =
        reinterpret_cast<Display*>(
            native.NativeDisplay());
    const Window window =
        static_cast<Window>(
            native.NativeDrawable());
    if (display == nullptr ||
        window == 0U) {
        return;
    }
    XStoreName(
        display,
        window,
        "AeroGUI ControlGallery");
    XMapWindow(display, window);
    XFlush(display);
    Atom close =
        XInternAtom(
            display,
            "WM_DELETE_WINDOW",
            False);
    static_cast<void>(
        XSetWMProtocols(
            display,
            window,
            &close,
            1));
    bool running = true;
    while (running) {
        XEvent event{};
        XNextEvent(display, &event);
        running =
            event.type != ClientMessage ||
            static_cast<Atom>(
                event.xclient.data.l[0]) !=
                close;
    }
}

} // namespace

Base::Result<void>
RunControlGalleryGlx(
    const Presentation::RenderPlan& plan,
    bool simulateContextLoss,
    bool interactive) noexcept {
    GlxSurfaceBackend native;
    SurfaceSession surface(native);
    NativeSurfaceDescriptor descriptor =
        MakeDescriptor();
    Base::Result<void> status =
        surface.Initialize(descriptor);
    if (status) {
        status = SubmitOpenGl(
            surface, native, plan);
    }
    if (status && simulateContextLoss) {
        status = surface.NotifyContextLost();
        if (status) {
            status = surface.Restore(
                descriptor);
        }
        if (status) {
            status = SubmitOpenGl(
                surface, native, plan);
        }
    }
    if (status && interactive) {
        RunMessageLoop(native);
    }
    surface.Shutdown();
    return status;
}

} // namespace Aero::Samples::ControlGallery

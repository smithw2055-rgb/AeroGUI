#include "GalleryRuntime.hpp"

#include <Aero/Rhi/D3D11Backend.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Rhi/WglSurface.hpp>
#include <Aero/Render/D3D11RendererBackend.hpp>
#include <Aero/Render/OpenGL33RendererBackend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Rhi;
using namespace Render;

class GalleryWindow final {
public:
    ~GalleryWindow() noexcept {
        if (window_ != nullptr &&
            IsWindow(window_) != FALSE) {
            static_cast<void>(
                DestroyWindow(window_));
        }
        if (atom_ != 0U &&
            instance_ != nullptr) {
            static_cast<void>(
                UnregisterClassW(
                    ClassName, instance_));
        }
    }

    Result<void> Initialize(
        bool visible) noexcept {
        instance_ = GetModuleHandleW(nullptr);
        if (instance_ == nullptr) {
            return Failure(
                "ControlGallery cannot resolve "
                "the Win32 module");
        }
        WNDCLASSEXW windowClass{};
        windowClass.cbSize =
            sizeof(windowClass);
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc =
            &GalleryWindow::WindowProcedure;
        windowClass.hInstance = instance_;
        windowClass.hCursor =
            LoadCursorW(
                nullptr,
                MAKEINTRESOURCEW(32512));
        windowClass.lpszClassName =
            ClassName;
        atom_ = RegisterClassExW(
            &windowClass);
        if (atom_ == 0U) {
            return Failure(
                "ControlGallery cannot register "
                "its Win32 window");
        }
        RECT bounds{
            0, 0,
            static_cast<LONG>(Width),
            static_cast<LONG>(Height)};
        static_cast<void>(AdjustWindowRect(
            &bounds,
            WS_OVERLAPPEDWINDOW,
            FALSE));
        window_ = CreateWindowExW(
            0U,
            ClassName,
            L"AeroGUI ControlGallery",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            nullptr,
            nullptr,
            instance_,
            nullptr);
        if (window_ == nullptr) {
            return Failure(
                "ControlGallery cannot create "
                "its Win32 window");
        }
        if (visible) {
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
        }
        return {};
    }

    void RunMessageLoop() noexcept {
        MSG message{};
        while (GetMessageW(
                   &message,
                   nullptr,
                   0U,
                   0U) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        window_ = nullptr;
    }

    HWND Handle() const noexcept {
        return window_;
    }

    static constexpr std::uint32_t Width =
        900U;
    static constexpr std::uint32_t Height =
        640U;

private:
    static constexpr wchar_t ClassName[] =
        L"AeroControlGalleryWindow";

    static Status Failure(
        const char* message) noexcept {
        return Status::Failure(
            ErrorCode::InternalError,
            message);
    }

    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept {
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(
            window, message, word, value);
    }

    HINSTANCE instance_ = nullptr;
    ATOM atom_ = 0U;
    HWND window_ = nullptr;
};

NativeSurfaceDescriptor
MakeD3D11Descriptor(
    D3D11GraphicsBackend& backend,
    HWND window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind =
        SurfaceKind::D3D11Window;
    descriptor.ownership =
        SurfaceOwnership::Owned;
    descriptor.presentMode =
        PresentMode::Immediate;
    descriptor.width =
        GalleryWindow::Width;
    descriptor.height =
        GalleryWindow::Height;
    descriptor.colorFormat =
        GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat =
        GraphicsTextureFormat::
            Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId =
        UINT64_C(0x43474433443131);
    descriptor.d3d11.window =
        reinterpret_cast<std::uintptr_t>(
            window);
    descriptor.d3d11.device =
        backend.NativeDevice();
    descriptor.d3d11.immediateContext =
        backend.NativeImmediateContext();
    return descriptor;
}

NativeSurfaceDescriptor
MakeWglDescriptor(HWND window) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind =
        SurfaceKind::WglWindow;
    descriptor.ownership =
        SurfaceOwnership::Owned;
    descriptor.presentMode =
        PresentMode::Immediate;
    descriptor.width =
        GalleryWindow::Width;
    descriptor.height =
        GalleryWindow::Height;
    descriptor.colorFormat =
        GraphicsTextureFormat::Bgra8Unorm;
    descriptor.stableId =
        UINT64_C(0x434757474C3333);
    descriptor.wgl.window =
        reinterpret_cast<std::uintptr_t>(
            window);
    return descriptor;
}

Result<void> SubmitOpenGl(
    SurfaceSession& surface,
    WglSurfaceBackend& native,
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
                result = renderer.Submit(
                    plan);
            }
            if (result) {
                result = backend.WaitForFence(
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

} // namespace

Base::Result<void>
RunControlGalleryD3D11(
    const Presentation::RenderPlan& plan,
    bool simulateContextLoss,
    bool interactive) noexcept {
    GalleryWindow window;
    Base::Result<void> status =
        window.Initialize(interactive);
    if (!status) {
        return status.GetStatus();
    }
    D3D11BackendOptions options;
    options.deviceMode =
        interactive
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
        D3D11SwapChainSurface native(
            backend);
        SurfaceSession surface(native);
        NativeSurfaceDescriptor descriptor =
            MakeD3D11Descriptor(
                backend, window.Handle());
        status = surface.Initialize(
            descriptor);
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
            status = renderer.Submit(plan);
        }
        if (status) {
            status = backend.WaitForFence(
                renderer.LastSubmittedFence());
        }
        if (status &&
            simulateContextLoss) {
            renderer.Shutdown();
            presenter.Shutdown();
            status =
                surface.NotifyContextLost();
            if (status) {
                status =
                    surface.Restore(
                        descriptor);
            }
            if (status) {
                status =
                    presenter.Initialize();
            }
            if (status) {
                status =
                    renderer.Initialize();
            }
            if (status) {
                status =
                    renderer.Submit(plan);
            }
            if (status) {
                status =
                    backend.WaitForFence(
                        renderer.
                            LastSubmittedFence());
            }
        }
        if (status && interactive) {
            window.RunMessageLoop();
        }
        renderer.Shutdown();
        presenter.Shutdown();
        surface.Shutdown();
    }
    backend.Shutdown();
    return status;
}

Base::Result<void>
RunControlGalleryWgl(
    const Presentation::RenderPlan& plan,
    bool simulateContextLoss,
    bool interactive) noexcept {
    GalleryWindow window;
    Base::Result<void> status =
        window.Initialize(interactive);
    if (!status) {
        return status.GetStatus();
    }
    WglSurfaceBackend native;
    SurfaceSession surface(native);
    NativeSurfaceDescriptor descriptor =
        MakeWglDescriptor(window.Handle());
    status = surface.Initialize(descriptor);
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
        window.RunMessageLoop();
    }
    surface.Shutdown();
    return status;
}

} // namespace Aero::Samples::ControlGallery

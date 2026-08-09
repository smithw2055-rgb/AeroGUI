#include "RenderContext.hpp"
#include "Presentation.hpp"

#include <AeroRender/OpenGL33.hpp>

#if defined(_WIN32)
#include "platform/win32/OpenGLWindow.hpp"
#elif defined(__linux__) || defined(__unix__)
#include "platform/x11/OpenGLWindow.hpp"
#endif

#include <new>
#include <utility>

namespace Aero::App {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

#if AERO_APP_HAS_OPENGL_WINDOW && \
    (defined(_WIN32) || defined(__linux__) || defined(__unix__))

#if defined(_WIN32)
using PlatformOpenGLWindow = Win32::OpenGLWindow;
#else
using PlatformOpenGLWindow = X11::OpenGLWindow;
#endif

class OpenGLRenderContext final : public RenderContext {
public:
    ~OpenGLRenderContext() noexcept override { Shutdown(); }

    Base::Result<void> Initialize(
        Platform::NativeWindowHandle window,
        std::uint32_t width,
        std::uint32_t height,
        Base::IAllocator* allocator) noexcept {
        Base::Result<void> native =
            window_.Initialize(window, {width, height});
        if (!native) return native.GetStatus();

        Render::OpenGL33::DeviceOptions deviceOptions;
        deviceOptions.resolve = &PlatformOpenGLWindow::ResolveCallback;
        deviceOptions.makeCurrent =
            &PlatformOpenGLWindow::MakeCurrentCallback;
        deviceOptions.isCurrent =
            &PlatformOpenGLWindow::IsCurrentCallback;
        deviceOptions.contextGeneration =
            &PlatformOpenGLWindow::GenerationCallback;
        deviceOptions.callbackContext = &window_;
        deviceOptions.statePolicy =
            Render::OpenGL33::StatePreservationPolicy::HostResetsState;
        Base::Result<Base::Ref<RenderDevice>> createdDevice =
            Render::OpenGL33::CreateDevice(deviceOptions, allocator);
        if (!createdDevice) {
            window_.Shutdown();
            return createdDevice.GetStatus();
        }

        Render::OpenGL33::TargetOptions targetOptions;
        targetOptions.acquireTarget = &AcquireTarget;
        targetOptions.callbackContext = this;
        targetOptions.targetContext = this;
        targetOptions.clearBeforeRender = true;
        Base::Result<Base::Ref<RenderTarget>> createdTarget =
            Render::OpenGL33::CreateTarget(
                std::move(createdDevice).Value(), targetOptions, allocator);
        if (!createdTarget) {
            window_.Shutdown();
            return createdTarget.GetStatus();
        }
        Base::Result<void> adopted =
            AdoptTarget(std::move(createdTarget).Value());
        if (!adopted) {
            window_.Shutdown();
            return adopted.GetStatus();
        }
        return {};
    }

protected:
    Base::Result<void> BeginPresentation() noexcept override {
        return window_.MakeCurrent();
    }

    Base::Result<void> ResizePresentation(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        Base::Result<void> resized = window_.Resize({width, height});
        if (resized) ++surfaceGeneration_;
        return resized;
    }

    Base::Result<void> PresentFrame() noexcept override {
        return window_.Present();
    }

    void CancelFrame() noexcept override {}

    void ShutdownPresentation() noexcept override {
        window_.Shutdown();
        surfaceGeneration_ = 1U;
    }

private:
    static Base::Status AcquireTarget(
        void* context,
        Render::OpenGL33::EmbeddedTarget* target) noexcept {
        auto* owner = static_cast<OpenGLRenderContext*>(context);
        if (owner == nullptr || target == nullptr ||
            owner->window_.Generation() == 0U ||
            !owner->window_.IsCurrent()) {
            return InvalidState(
                "OpenGL application target was requested without a current window context");
        }
        target->framebuffer = 0U;
        target->width = owner->window_.Width();
        target->height = owner->window_.Height();
        target->stableId =
            owner->window_.StableId() ^ owner->surfaceGeneration_;
        target->defaultFramebuffer = true;
        return Base::Status::Ok();
    }

    PlatformOpenGLWindow window_;
    std::uint64_t surfaceGeneration_ = 1U;
};

#endif

} // namespace

Base::Result<RenderContext*> CreateOpenGL33RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator) noexcept {
#if AERO_APP_HAS_OPENGL_WINDOW && \
    (defined(_WIN32) || defined(__linux__) || defined(__unix__))
    auto* context = new (std::nothrow) OpenGLRenderContext();
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the OpenGL application context");
    }
    Base::Result<void> initialized =
        context->Initialize(window, width, height, allocator);
    if (!initialized) {
        delete context;
        return initialized.GetStatus();
    }
    return static_cast<RenderContext*>(context);
#else
    static_cast<void>(window);
    static_cast<void>(width);
    static_cast<void>(height);
    static_cast<void>(allocator);
    return Unsupported("OpenGL application backend is not enabled");
#endif
}

} // namespace Aero::App

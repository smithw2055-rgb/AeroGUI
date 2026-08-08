#include "RenderContext.hpp"
#include "render/private/RenderDevice.hpp"
#include "render/private/RenderTarget.hpp"

#include <new>
#include <utility>

namespace Aero::App::Detail {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

} // namespace

Base::Result<void> RenderContext::AdoptTarget(
    Base::Ref<RenderTarget> target) noexcept {
    Shutdown();
    if (!target || target->State() != RenderTargetState::Ready) {
        return InvalidState("Render context requires a ready target");
    }
    target_ = std::move(target);
    return {};
}

class D3D11RenderContext final : public RenderContext {
public:
    Base::Result<void> Initialize(
        Platform::NativeWindowHandle window,
        std::uint32_t width,
        std::uint32_t height,
        Base::IAllocator* allocator) noexcept {
#if defined(_WIN32) && AERO_APP_HAS_D3D11
        Render::D3D11DeviceOptions deviceOptions;
        deviceOptions.allowWarpFallback = true;
        Base::Result<Base::Ref<RenderDevice>> device =
            Render::Detail::CreateD3D11Device(deviceOptions, allocator);
        if (!device) return device.GetStatus();
        Render::Detail::D3D11WindowTargetOptions options;
        options.window = window;
        options.width = width;
        options.height = height;
        Base::Result<Base::Ref<RenderTarget>> created =
            Render::Detail::CreateD3D11WindowTarget(
                std::move(device).Value(), options, allocator);
        return created
            ? AdoptTarget(std::move(created).Value())
            : Base::Result<void>(created.GetStatus());
#else
        static_cast<void>(window);
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(allocator);
        return Unsupported("D3D11 application backend is not enabled");
#endif
    }
};

class GLRenderContext final : public RenderContext {
public:
    Base::Result<void> Initialize(
        Platform::NativeWindowHandle window,
        std::uint32_t width,
        std::uint32_t height,
        Base::IAllocator* allocator) noexcept {
#if AERO_APP_HAS_OPENGL_WINDOW
        Render::Detail::OpenGL33WindowTargetOptions options;
        options.window = window;
        options.width = width;
        options.height = height;
        Base::Result<Render::Detail::WindowRenderPair> pair =
            Render::Detail::CreateOpenGL33WindowRenderPair(options, allocator);
        if (!pair) return pair.GetStatus();
        Render::Detail::WindowRenderPair created = std::move(pair).Value();
        return AdoptTarget(std::move(created.target));
#else
        static_cast<void>(window);
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(allocator);
        return Unsupported("OpenGL application backend is not enabled");
#endif
    }
};

Base::Result<RenderContext*> CreateRenderContext(
    GraphicsBackend backend,
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator) noexcept {
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render context dimensions must be nonzero");
    }

    GraphicsBackend selected = backend;
    if (selected == GraphicsBackend::Automatic) {
#if defined(_WIN32)
        selected = GraphicsBackend::D3D11;
#else
        selected = GraphicsBackend::OpenGL33;
#endif
    }

    if (selected == GraphicsBackend::D3D11) {
        auto* context = new (std::nothrow) D3D11RenderContext();
        if (context == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Unable to allocate the D3D11 render context");
        }
        Base::Result<void> initialized = context->Initialize(
            window, width, height, allocator);
        if (!initialized) {
            delete context;
            return initialized.GetStatus();
        }
        return static_cast<RenderContext*>(context);
    }

    if (selected == GraphicsBackend::OpenGL33) {
        auto* context = new (std::nothrow) GLRenderContext();
        if (context == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Unable to allocate the OpenGL render context");
        }
        Base::Result<void> initialized = context->Initialize(
            window, width, height, allocator);
        if (!initialized) {
            delete context;
            return initialized.GetStatus();
        }
        return static_cast<RenderContext*>(context);
    }

    return Unsupported("Requested application graphics backend is unavailable");
}

Base::Result<void> RenderContext::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (!target_) {
        return InvalidState("Application render context is not initialized");
    }
    if (frameOpen_) {
        return InvalidState("Application render context cannot resize during a frame");
    }
    return target_->Resize(width, height);
}

Base::Result<void> RenderContext::BeginFrame() noexcept {
    if (!IsReady() || frameOpen_) {
        return InvalidState(frameOpen_
            ? "Application render context already has an open frame"
            : "Application render context is unavailable");
    }
    Base::Result<void> begun = RenderTarget::Impl::BeginFrame(*target_);
    if (!begun) return begun.GetStatus();
    currentTarget_ = target_.Get();
    frameOpen_ = true;
    return {};
}

Base::Result<void> RenderContext::EndFrame() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr) {
        return InvalidState("Application render context has no open frame");
    }
    return RenderTarget::Impl::EndFrame(*currentTarget_);
}

Base::Result<void> RenderContext::Present() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr) {
        return InvalidState("Application render context has no frame to present");
    }
    Base::Result<void> presented =
        RenderTarget::Impl::Present(*currentTarget_);
    currentTarget_ = nullptr;
    frameOpen_ = false;
    return presented;
}

Base::Result<void> RenderContext::Render(IRenderer& renderer) noexcept {
    Base::Result<void> begun = BeginFrame();
    if (!begun) return begun.GetStatus();

    Base::Result<void> rendered = renderer.Render(*currentTarget_);
    if (!rendered) {
        RenderTarget::Impl::CancelFrame(*currentTarget_);
        currentTarget_ = nullptr;
        frameOpen_ = false;
        return rendered.GetStatus();
    }

    Base::Result<void> ended = EndFrame();
    if (!ended) {
        RenderTarget::Impl::CancelFrame(*currentTarget_);
        currentTarget_ = nullptr;
        frameOpen_ = false;
        return ended.GetStatus();
    }
    return Present();
}

void RenderContext::Shutdown() noexcept {
    if (frameOpen_ && currentTarget_ != nullptr) {
        RenderTarget::Impl::CancelFrame(*currentTarget_);
    }
    currentTarget_ = nullptr;
    frameOpen_ = false;
    if (target_) {
        Base::Ref<RenderDevice> device = target_->GetDevice();
        if (device) static_cast<void>(device->WaitIdle());
    }
    target_.Reset();
}

} // namespace Aero::App::Detail

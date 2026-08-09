#include "RenderContext.hpp"

#include <utility>

namespace Aero::App {
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
    if (target_ || !target || target->State() != RenderTargetState::Ready) {
        return InvalidState("Render context requires a ready target");
    }
    target_ = std::move(target);
    return {};
}

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
        return CreateD3D11RenderContext(
            window, width, height, allocator);
    }

    if (selected == GraphicsBackend::OpenGL33) {
        return CreateOpenGL33RenderContext(
            window, width, height, allocator);
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
    Base::Ref<RenderDevice> device = target_->GetDevice();
    if (device) {
        Base::Result<void> idle = device->WaitIdle();
        if (!idle) return idle.GetStatus();
    }
    return ResizePresentation(width, height);
}

Base::Result<void> RenderContext::BeginFrame() noexcept {
    if (!IsReady() || frameOpen_) {
        return InvalidState(frameOpen_
            ? "Application render context already has an open frame"
            : "Application render context is unavailable");
    }
    Base::Result<void> begun = BeginPresentation();
    if (!begun) return begun.GetStatus();
    currentTarget_ = target_.Get();
    frameOpen_ = true;
    frameRendered_ = false;
    frameEnded_ = false;
    return {};
}

Base::Result<void> RenderContext::EndFrame() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr || !frameRendered_) {
        return InvalidState("Application render context has no open frame");
    }
    if (frameEnded_) {
        return InvalidState("Application render context frame already ended");
    }
    frameEnded_ = true;
    return {};
}

Base::Result<void> RenderContext::Present() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr || !frameEnded_) {
        return InvalidState("Application render context has no frame to present");
    }
    Base::Result<void> presented = PresentFrame();
    currentTarget_ = nullptr;
    frameOpen_ = false;
    frameRendered_ = false;
    frameEnded_ = false;
    return presented;
}

Base::Result<void> RenderContext::Render(IRenderer& renderer) noexcept {
    Base::Result<void> begun = BeginFrame();
    if (!begun) return begun.GetStatus();

    renderer.Render(*currentTarget_);
    if (currentTarget_->State() != RenderTargetState::Ready) {
        CancelFrame();
        currentTarget_ = nullptr;
        frameOpen_ = false;
        frameRendered_ = false;
        frameEnded_ = false;
        return InvalidState(
            "Renderer left the application render target unavailable");
    }
    frameRendered_ = true;

    Base::Result<void> ended = EndFrame();
    if (!ended) {
        CancelFrame();
        currentTarget_ = nullptr;
        frameOpen_ = false;
        frameRendered_ = false;
        frameEnded_ = false;
        return ended.GetStatus();
    }
    return Present();
}

void RenderContext::Shutdown() noexcept {
    if (frameOpen_ && currentTarget_ != nullptr) {
        CancelFrame();
    }
    currentTarget_ = nullptr;
    frameOpen_ = false;
    frameRendered_ = false;
    frameEnded_ = false;
    if (target_) {
        Base::Ref<RenderDevice> device = target_->GetDevice();
        if (device) static_cast<void>(device->WaitIdle());
    }
    target_.Reset();
    ShutdownPresentation();
}

} // namespace Aero::App

#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/IRenderer.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/WindowInterop.hpp>
#include "render/RenderDeviceMaintenance.hpp"

#include <cstdint>
#include <utility>

namespace Aero::Render {

// Source-private render presentation owner. Backend-specific contexts create
// the native device/target pair; this base owns the common frame and Present
// state. It is header-only so both native backend products and the desktop App
// share one lifecycle contract without a DLL-visible symbol.
class RenderContext {
public:
    RenderContext() noexcept = default;
    virtual ~RenderContext() noexcept = default;

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<void> BeginFrame() noexcept;
    Base::Result<void> EndFrame() noexcept;
    Base::Result<void> Present() noexcept;
    Base::Result<void> Render(IRenderer& renderer) noexcept;
    void Shutdown() noexcept;

    bool IsReady() const noexcept {
        return target_ && target_->State() == RenderTargetState::Ready;
    }
    Base::Ref<RenderDevice> Device() const noexcept {
        return target_ ? target_->GetDevice() : Base::Ref<RenderDevice>{};
    }
    RenderTarget* Target() noexcept { return target_.Get(); }
    const RenderTarget* Target() const noexcept { return target_.Get(); }

protected:
    Base::Result<void> AdoptTarget(
        Base::Ref<RenderTarget> target) noexcept;

    virtual Base::Result<void> BeginPresentation() noexcept = 0;
    virtual Base::Result<void> ResizePresentation(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual Base::Result<void> PresentFrame() noexcept = 0;
    virtual void CancelFrame() noexcept = 0;
    virtual void ShutdownPresentation() noexcept = 0;

private:
    static Base::Status PresentationInvalidState(const char* message) noexcept {
        return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
    }

    Base::Ref<RenderTarget> target_;
    RenderTarget* currentTarget_ = nullptr;
    bool frameOpen_ = false;
    bool frameRendered_ = false;
    bool frameEnded_ = false;
};

inline Base::Result<void> RenderContext::AdoptTarget(
    Base::Ref<RenderTarget> target) noexcept {
    if (target_ || !target || target->State() != RenderTargetState::Ready) {
        return PresentationInvalidState("Render context requires a ready target");
    }
    target_ = std::move(target);
    return {};
}

inline Base::Result<void> RenderContext::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (!target_) {
        return PresentationInvalidState("Application render context is not initialized");
    }
    if (frameOpen_) {
        return PresentationInvalidState("Application render context cannot resize during a frame");
    }
    Base::Ref<RenderDevice> device = target_->GetDevice();
    if (device) {
        Base::Result<void> idle = device->WaitIdle();
        if (!idle) return idle.GetStatus();
        Base::Result<std::uint32_t> collected =
            CollectDeviceGarbage(*device);
        if (!collected) return collected.GetStatus();
    }
    return ResizePresentation(width, height);
}

inline Base::Result<void> RenderContext::BeginFrame() noexcept {
    if (!IsReady() || frameOpen_) {
        return PresentationInvalidState(frameOpen_
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

inline Base::Result<void> RenderContext::EndFrame() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr || !frameRendered_) {
        return PresentationInvalidState("Application render context has no open frame");
    }
    if (frameEnded_) {
        return PresentationInvalidState("Application render context frame already ended");
    }
    frameEnded_ = true;
    return {};
}

inline Base::Result<void> RenderContext::Present() noexcept {
    if (!frameOpen_ || currentTarget_ == nullptr || !frameEnded_) {
        return PresentationInvalidState("Application render context has no frame to present");
    }
    Base::Result<void> presented = PresentFrame();
    currentTarget_ = nullptr;
    frameOpen_ = false;
    frameRendered_ = false;
    frameEnded_ = false;
    return presented;
}

inline Base::Result<void> RenderContext::Render(IRenderer& renderer) noexcept {
    Base::Result<void> begun = BeginFrame();
    if (!begun) return begun.GetStatus();

    renderer.Render(*currentTarget_);
    if (currentTarget_->State() != RenderTargetState::Ready) {
        CancelFrame();
        currentTarget_ = nullptr;
        frameOpen_ = false;
        frameRendered_ = false;
        frameEnded_ = false;
        return PresentationInvalidState(
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

inline void RenderContext::Shutdown() noexcept {
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

// Creates the concrete desktop context selected by a backend. Ownership of a
// successful result transfers to the caller. Implementations live in the
// matching native backend product.
Base::Result<RenderContext*> CreateD3D11RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<RenderContext*> CreateOpenGL33RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render
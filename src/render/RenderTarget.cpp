#include "render/private/RenderTarget.hpp"

#include <utility>

namespace Aero {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

} // namespace

RenderTarget::RenderTarget(
    ConstructionToken,
    Base::Ref<Aero::RenderDevice> device,
    Impl* implementation) noexcept
    : device_(std::move(device)),
      impl_(implementation) {}

RenderTarget::~RenderTarget() noexcept {
    delete impl_;
    impl_ = nullptr;
    device_.Reset();
}

RenderTargetKind RenderTarget::Kind() const noexcept {
    return impl_ != nullptr ? impl_->kind : RenderTargetKind::Embedded;
}

RenderTargetState RenderTarget::State() const noexcept {
    if (impl_ == nullptr || !device_) {
        return RenderTargetState::Shutdown;
    }
    switch (device_->State()) {
    case Aero::RenderDeviceState::Ready:
        break;
    case Aero::RenderDeviceState::DeviceLost:
        return RenderTargetState::DeviceLost;
    case Aero::RenderDeviceState::Failed:
        return RenderTargetState::Failed;
    case Aero::RenderDeviceState::Shutdown:
        return RenderTargetState::Shutdown;
    }
    switch (impl_->GetSurfaceHealth()) {
    case ::Aero::Render::Detail::SurfaceHealth::Ready:
        return RenderTargetState::Ready;
    case ::Aero::Render::Detail::SurfaceHealth::Lost:
        return RenderTargetState::Lost;
    case ::Aero::Render::Detail::SurfaceHealth::Failed:
        return RenderTargetState::Failed;
    case ::Aero::Render::Detail::SurfaceHealth::Shutdown:
        return RenderTargetState::Shutdown;
    }
    return RenderTargetState::Failed;
}

Base::Ref<Aero::RenderDevice> RenderTarget::GetDevice() const noexcept {
    return device_;
}

Base::Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || !device_) {
        return NotInitialized("Render target is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Base::Result<void> idle = device_->WaitIdle();
    if (!idle) return idle.GetStatus();
    return impl_->Resize(width, height);
}

void RenderTarget::NotifyLost() noexcept {
    if (impl_ == nullptr || !device_ ||
        device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return;
    }
    impl_->NotifySurfaceLost();
    Aero::RenderDevice::Impl::RefreshHealth(*device_);
}

Base::Result<void> RenderTarget::Restore() noexcept {
    if (impl_ == nullptr || !device_) {
        return NotInitialized("Render target is not initialized");
    }
    if (device_->State() != Aero::RenderDeviceState::Ready ||
        impl_->GetSurfaceHealth() !=
            ::Aero::Render::Detail::SurfaceHealth::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Base::Result<void> restored = impl_->RestoreSurface();
    Aero::RenderDevice::Impl::RefreshHealth(*device_);
    return restored;
}

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Impl::Create(
    Base::Ref<Aero::RenderDevice> device,
    Impl* implementation,
    Base::IAllocator* allocator) noexcept {
    if (!device || implementation == nullptr) {
        delete implementation;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target requires a device and implementation");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderTarget>> made =
        Base::MakeRefWithAllocator<RenderTarget>(
            selected,
            RenderTarget::ConstructionToken{},
            std::move(device),
            implementation);
    if (!made) delete implementation;
    return made;
}

Base::Result<void> RenderTarget::Impl::Render(
    RenderTarget& target,
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    if (target.impl_ == nullptr || !target.device_) {
        return NotInitialized("Render target is not initialized");
    }
    Base::Status deviceReady =
        Aero::RenderDevice::Impl::FrameStatus(*target.device_);
    if (!deviceReady.IsOk()) return deviceReady;
    if (target.impl_->GetSurfaceHealth() !=
            ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render target is not ready");
    }

    Base::Result<Aero::RenderFrameStatistics> statistics =
        Aero::RenderDevice::Impl::BeginSurfaceFrame(*target.device_, frame);
    if (!statistics) return statistics.GetStatus();

    Base::Result<void> rendered =
        target.impl_->Render(rendererToken, frame);
    if (!rendered) {
        Aero::RenderDevice::Impl::RecordSurfaceFailure(*target.device_);
        return rendered.GetStatus();
    }

    if (target.impl_->frameOpen) {
        target.impl_->frameRendered = true;
    }

    Aero::RenderDevice::Impl::CompleteSurfaceFrame(
        *target.device_, frame, statistics.Value());
    return {};
}

Base::Result<void> RenderTarget::Impl::PresentFrame() noexcept {
    return InvalidState("Render target does not support desktop presentation");
}

void RenderTarget::Impl::DiscardFrame() noexcept {}

Base::Result<void> RenderTarget::Impl::BeginFrame(
    RenderTarget& target) noexcept {
    if (target.impl_ == nullptr || !target.device_) {
        return NotInitialized("Render target is not initialized");
    }
    if (target.impl_->kind != RenderTargetKind::Window) {
        return InvalidState("Only window render targets have a desktop frame lifecycle");
    }
    if (target.State() != RenderTargetState::Ready) {
        return InvalidState("Render target is not ready to begin a frame");
    }
    if (target.impl_->frameOpen) {
        return InvalidState("Render target already has an open frame");
    }
    target.impl_->frameOpen = true;
    target.impl_->frameRendered = false;
    target.impl_->frameEnded = false;
    return {};
}

Base::Result<void> RenderTarget::Impl::EndFrame(
    RenderTarget& target) noexcept {
    if (target.impl_ == nullptr || !target.impl_->frameOpen) {
        return InvalidState("Render target has no open frame");
    }
    if (!target.impl_->frameRendered) {
        return InvalidState("Render target frame has not been rendered");
    }
    if (target.impl_->frameEnded) {
        return InvalidState("Render target frame has already ended");
    }
    target.impl_->frameEnded = true;
    return {};
}

Base::Result<void> RenderTarget::Impl::Present(
    RenderTarget& target) noexcept {
    if (target.impl_ == nullptr || !target.impl_->frameOpen ||
        !target.impl_->frameEnded) {
        return InvalidState("Render target frame must end before present");
    }
    Base::Result<void> presented = target.impl_->PresentFrame();
    if (!presented) {
        target.impl_->DiscardFrame();
        Aero::RenderDevice::Impl::RecordSurfaceFailure(*target.device_);
    }
    target.impl_->frameOpen = false;
    target.impl_->frameRendered = false;
    target.impl_->frameEnded = false;
    return presented;
}

void RenderTarget::Impl::CancelFrame(RenderTarget& target) noexcept {
    if (target.impl_ == nullptr || !target.impl_->frameOpen) return;
    target.impl_->DiscardFrame();
    target.impl_->frameOpen = false;
    target.impl_->frameRendered = false;
    target.impl_->frameEnded = false;
}

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Impl* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (target != nullptr) target->kind = kind;
    return Aero::RenderTarget::Impl::Create(
        std::move(device), target, allocator);
}

} // namespace Aero::Render::Detail

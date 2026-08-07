#include "render/private/BackendApi.hpp"
#include "render/private/RenderTarget.hpp"

#include <utility>

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33WindowDevice(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;

} // namespace Aero::Render::Detail

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
    Impl* implementation,
    bool ownsImplementation) noexcept
    : device_(std::move(device)),
      impl_(implementation),
      ownsImpl_(ownsImplementation) {}

RenderTarget::~RenderTarget() noexcept {
    if (ownsImpl_) delete impl_;
    impl_ = nullptr;
    ownsImpl_ = false;
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

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Impl::CreateBorrowed(
    Base::Ref<Aero::RenderDevice> device,
    Impl* implementation,
    Base::IAllocator* allocator) noexcept {
    if (!device || implementation == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target requires a device and implementation");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    return Base::MakeRefWithAllocator<RenderTarget>(
        selected,
        RenderTarget::ConstructionToken{},
        std::move(device),
        implementation,
        false);
}

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Impl::CreateOwned(
    Base::Ref<Aero::RenderDevice> device,
    Impl* implementation,
    Base::IAllocator* allocator) noexcept {
    if (!device || implementation == nullptr) {
        delete implementation;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Owned render target requires a device and implementation");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderTarget>> made =
        Base::MakeRefWithAllocator<RenderTarget>(
            selected,
            RenderTarget::ConstructionToken{},
            std::move(device),
            implementation,
            true);
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

    Aero::RenderDevice::Impl::CompleteSurfaceFrame(
        *target.device_, frame, statistics.Value());
    return {};
}

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target requires a render device");
    }
    Aero::RenderTarget::Impl* implementation =
        Aero::RenderDevice::Impl::DefaultTarget(*device);
    if (implementation == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render device has no default target");
    }
    implementation->kind = kind;
    return Aero::RenderTarget::Impl::CreateBorrowed(
        std::move(device), implementation, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Impl* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (target != nullptr) target->kind = kind;
    return Aero::RenderTarget::Impl::CreateOwned(
        std::move(device), target, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    return AdoptRenderTarget(std::move(device), kind, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Impl* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    return AdoptOwnedRenderTarget(std::move(device), target, kind, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateOpenGL33WindowDevice(options, allocator);
    if (!device) return device.GetStatus();
    return AdoptRenderTarget(
        std::move(device).Value(), Aero::RenderTargetKind::Window, allocator);
}

} // namespace Aero::Render::Detail

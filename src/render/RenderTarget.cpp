#include "render/private/BackendApi.hpp"
#include "render/private/RenderTarget.hpp"

#include <new>
#include <utility>

namespace Aero::Render::Detail {

// The remaining OpenGL window path creates its native context and target as one
// object. Embedded OpenGL and all D3D11 paths use independently owned targets.
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
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    void* memory = selected.Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    }
    impl_ = new (memory) Impl(std::move(device), kind, selected);
}

RenderTarget::~RenderTarget() noexcept {
    if (impl_ == nullptr) return;
    Base::IAllocator* allocator = impl_->allocator;
    impl_->~Impl();
    allocator->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

RenderTargetKind RenderTarget::Kind() const noexcept {
    return impl_ != nullptr ? impl_->kind : RenderTargetKind::Embedded;
}

RenderTargetState RenderTarget::State() const noexcept {
    if (impl_ == nullptr || !impl_->device) {
        return RenderTargetState::Shutdown;
    }
    switch (impl_->device->State()) {
    case Aero::RenderDeviceState::Ready:
        break;
    case Aero::RenderDeviceState::DeviceLost:
        return RenderTargetState::DeviceLost;
    case Aero::RenderDeviceState::Failed:
        return RenderTargetState::Failed;
    case Aero::RenderDeviceState::Shutdown:
        return RenderTargetState::Shutdown;
    }
    switch (impl_->RefreshHealth()) {
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
    return impl_ != nullptr
        ? impl_->device
        : Base::Ref<Aero::RenderDevice>{};
}

Base::Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr) {
        return NotInitialized("Render target is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Base::Result<void> idle = impl_->device->WaitIdle();
    if (!idle) return idle.GetStatus();
    Base::Result<void> resized = impl_->target->Resize(width, height);
    if (!resized) impl_->RefreshHealth();
    return resized;
}

void RenderTarget::NotifyLost() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr ||
        impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return;
    }
    impl_->health = ::Aero::Render::Detail::SurfaceHealth::Lost;
    impl_->target->NotifySurfaceLost();
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
}

Base::Result<void> RenderTarget::Restore() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr) {
        return NotInitialized("Render target is not initialized");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != ::Aero::Render::Detail::SurfaceHealth::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Base::Result<void> restored = impl_->target->RestoreSurface();
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
    return restored;
}

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Impl::Create(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device || Aero::RenderDevice::Impl::DefaultTarget(*device) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target requires a target-capable render device");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    return Base::MakeRefWithAllocator<RenderTarget>(
        selected,
        RenderTarget::ConstructionToken{},
        std::move(device),
        kind,
        &selected);
}

Base::Result<Base::Ref<RenderTarget>> RenderTarget::Impl::CreateOwned(
    Base::Ref<Aero::RenderDevice> device,
    ::Aero::Render::Detail::NativeRenderTarget* target,
    RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device || target == nullptr) {
        delete target;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Owned render target requires device and native target");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderTarget>> made =
        Base::MakeRefWithAllocator<RenderTarget>(
            selected,
            RenderTarget::ConstructionToken{},
            std::move(device),
            kind,
            &selected);
    if (!made) {
        delete target;
        return made.GetStatus();
    }
    made.Value()->impl_->SetTarget(target, true);
    return std::move(made).Value();
}

Base::Result<void> RenderTarget::Impl::Render(
    RenderTarget& target,
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    Impl* impl = target.impl_;
    if (impl == nullptr || !impl->device || impl->target == nullptr) {
        return NotInitialized("Render target is not initialized");
    }
    Base::Status deviceReady =
        Aero::RenderDevice::Impl::FrameStatus(*impl->device);
    if (!deviceReady.IsOk()) return deviceReady;
    if (impl->RefreshHealth() != ::Aero::Render::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render target is not ready");
    }

    Base::Result<Aero::RenderFrameStatistics> statistics =
        Aero::RenderDevice::Impl::BeginSurfaceFrame(*impl->device, frame);
    if (!statistics) return statistics.GetStatus();

    Base::Result<void> rendered = impl->target->Render(rendererToken, frame);
    if (!rendered) {
        Aero::RenderDevice::Impl::RecordSurfaceFailure(*impl->device);
        impl->RefreshHealth();
        return rendered.GetStatus();
    }

    Aero::RenderDevice::Impl::CompleteSurfaceFrame(
        *impl->device, frame, statistics.Value());
    return {};
}

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    return Aero::RenderTarget::Impl::Create(
        std::move(device), kind, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    return Aero::RenderTarget::Impl::CreateOwned(
        std::move(device), target, kind, allocator);
}

// Temporary source-only names used by backend implementation files. The public
// API and install tree have no RenderSurface type.
Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator) noexcept {
    return AdoptRenderTarget(std::move(device), kind, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
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

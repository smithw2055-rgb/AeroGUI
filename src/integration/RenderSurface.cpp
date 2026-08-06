#include "integration/IntegrationPrivate.hpp"
#include "integration/private/RenderSurface.hpp"

#include <new>
#include <utility>

namespace Aero::Integration {

// The remaining OpenGL window path creates its native context and target as one
// object. Embedded OpenGL and all D3D11 paths use independently owned targets.
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33WindowDevice(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;

} // namespace Aero::Integration

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

RenderSurface::RenderSurface(
    ConstructionToken,
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderSurfaceKind kind,
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

RenderSurface::~RenderSurface() noexcept {
    if (impl_ == nullptr) return;
    Base::IAllocator* allocator = impl_->allocator;
    impl_->~Impl();
    allocator->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

RenderSurfaceKind RenderSurface::Kind() const noexcept {
    return impl_ != nullptr ? impl_->kind : RenderSurfaceKind::Embedded;
}

RenderSurfaceState RenderSurface::State() const noexcept {
    if (impl_ == nullptr || !impl_->device) {
        return RenderSurfaceState::Shutdown;
    }
    switch (impl_->device->State()) {
    case Aero::RenderDeviceState::Ready:
        break;
    case Aero::RenderDeviceState::DeviceLost:
        return RenderSurfaceState::DeviceLost;
    case Aero::RenderDeviceState::Failed:
        return RenderSurfaceState::Failed;
    case Aero::RenderDeviceState::Shutdown:
        return RenderSurfaceState::Shutdown;
    }
    switch (impl_->RefreshHealth()) {
    case Integration::Detail::SurfaceHealth::Ready:
        return RenderSurfaceState::Ready;
    case Integration::Detail::SurfaceHealth::Lost:
        return RenderSurfaceState::Lost;
    case Integration::Detail::SurfaceHealth::Failed:
        return RenderSurfaceState::Failed;
    case Integration::Detail::SurfaceHealth::Shutdown:
        return RenderSurfaceState::Shutdown;
    }
    return RenderSurfaceState::Failed;
}

Base::Ref<Aero::RenderDevice> RenderSurface::GetDevice() const noexcept {
    return impl_ != nullptr
        ? impl_->device
        : Base::Ref<Aero::RenderDevice>{};
}

Base::Result<void> RenderSurface::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render surface dimensions must be nonzero");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Integration::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render surface cannot resize in its current state");
    }
    Base::Result<void> idle = impl_->device->WaitIdle();
    if (!idle) return idle.GetStatus();
    Base::Result<void> resized = impl_->target->Resize(width, height);
    if (!resized) impl_->RefreshHealth();
    return resized;
}

void RenderSurface::NotifyLost() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr ||
        impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Integration::Detail::SurfaceHealth::Ready) {
        return;
    }
    impl_->health = Integration::Detail::SurfaceHealth::Lost;
    impl_->target->NotifySurfaceLost();
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
}

Base::Result<void> RenderSurface::Restore() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->target == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Integration::Detail::SurfaceHealth::Lost) {
        return InvalidState("Only a lost render surface can be restored");
    }
    Base::Result<void> restored = impl_->target->RestoreSurface();
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
    return restored;
}

Base::Result<Base::Ref<RenderSurface>> RenderSurface::Impl::Create(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device || Aero::RenderDevice::Impl::DefaultTarget(*device) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render surface requires a target-capable render device");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    return Base::MakeRefWithAllocator<RenderSurface>(
        selected,
        RenderSurface::ConstructionToken{},
        std::move(device),
        kind,
        &selected);
}

Base::Result<Base::Ref<RenderSurface>> RenderSurface::Impl::CreateOwned(
    Base::Ref<Aero::RenderDevice> device,
    Integration::Detail::NativeRenderTarget* target,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device || target == nullptr) {
        delete target;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Owned render surface requires device and native target");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator
        : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<RenderSurface>> made =
        Base::MakeRefWithAllocator<RenderSurface>(
            selected,
            RenderSurface::ConstructionToken{},
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

Base::Result<void> RenderSurface::Impl::Render(
    RenderSurface& surface,
    const void* rendererToken,
    const Integration::RenderFrame& frame) noexcept {
    Impl* impl = surface.impl_;
    if (impl == nullptr || !impl->device || impl->target == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    Base::Status deviceReady =
        Aero::RenderDevice::Impl::FrameStatus(*impl->device);
    if (!deviceReady.IsOk()) return deviceReady;
    if (impl->RefreshHealth() != Integration::Detail::SurfaceHealth::Ready) {
        return InvalidState("Render surface is not ready");
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

namespace Aero::Integration::Detail {

Base::Result<Base::Ref<Aero::RenderSurface>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    return Aero::RenderSurface::Impl::Create(
        std::move(device), kind, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
    Aero::RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    return Aero::RenderSurface::Impl::CreateOwned(
        std::move(device), target, kind, allocator);
}

} // namespace Aero::Integration::Detail

namespace Aero::Integration {

Base::Result<Base::Ref<Aero::RenderSurface>> CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateOpenGL33WindowDevice(options, allocator);
    if (!device) return device.GetStatus();
    return Detail::AdoptRenderSurface(
        std::move(device).Value(), Aero::RenderSurfaceKind::Window, allocator);
}

} // namespace Aero::Integration

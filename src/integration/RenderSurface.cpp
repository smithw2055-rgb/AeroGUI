#include <Aero/Integration/D3D11.hpp>
#include <Aero/Integration/OpenGL33.hpp>

#include "integration/IntegrationPrivate.hpp"
#include "integration/private/RenderSurface.hpp"

#include <new>
#include <utility>

namespace Aero::Integration {
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

// Source-private combined constructors retained until native Device and Surface
// allocation are physically separated in each backend implementation.
#if defined(_WIN32)
Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11EmbeddedDevice(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;
Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11WindowDevice(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;
#endif
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33EmbeddedDevice(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33WindowDevice(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept;

RenderSurface::RenderSurface(
    ConstructionToken,
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
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
    case Detail::SurfaceHealth::Ready:
        return RenderSurfaceState::Ready;
    case Detail::SurfaceHealth::Lost:
        return RenderSurfaceState::Lost;
    case Detail::SurfaceHealth::Failed:
        return RenderSurfaceState::Failed;
    case Detail::SurfaceHealth::Shutdown:
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
    if (impl_ == nullptr || !impl_->device || impl_->stateData == nullptr ||
        impl_->functions == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render surface dimensions must be nonzero");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Detail::SurfaceHealth::Ready) {
        return InvalidState("Render surface cannot resize in its current state");
    }
    Base::Result<void> idle = impl_->device->WaitIdle();
    if (!idle) return idle.GetStatus();
    Base::Result<void> resized =
        impl_->functions->resize(impl_->stateData, width, height);
    if (!resized) impl_->RefreshHealth();
    return resized;
}

void RenderSurface::NotifyLost() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->stateData == nullptr ||
        impl_->functions == nullptr ||
        impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Detail::SurfaceHealth::Ready) {
        return;
    }
    impl_->health = Detail::SurfaceHealth::Lost;
    impl_->functions->surfaceLost(impl_->stateData);
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
}

Base::Result<void> RenderSurface::Restore() noexcept {
    if (impl_ == nullptr || !impl_->device || impl_->stateData == nullptr ||
        impl_->functions == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    if (impl_->device->State() != Aero::RenderDeviceState::Ready ||
        impl_->RefreshHealth() != Detail::SurfaceHealth::Lost) {
        return InvalidState("Only a lost render surface can be restored");
    }
    Base::Result<void> restored =
        impl_->functions->restoreSurface(impl_->stateData);
    Aero::RenderDevice::Impl::RefreshHealth(*impl_->device);
    impl_->RefreshHealth();
    return restored;
}

Base::Result<Base::Ref<RenderSurface>> RenderSurface::Impl::Create(
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device || Aero::RenderDevice::Impl::NativeState(*device) == nullptr ||
        Aero::RenderDevice::Impl::SurfaceFunctions(*device) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render surface requires a surface-capable render device");
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

Base::Result<void> RenderSurface::Impl::Render(
    RenderSurface& surface,
    const void* rendererToken,
    const Integration::RenderFrame& frame) noexcept {
    Impl* impl = surface.impl_;
    if (impl == nullptr || !impl->device || impl->stateData == nullptr ||
        impl->functions == nullptr) {
        return NotInitialized("Render surface is not initialized");
    }
    Base::Status deviceReady =
        Aero::RenderDevice::Impl::FrameStatus(*impl->device);
    if (!deviceReady.IsOk()) return deviceReady;
    if (impl->RefreshHealth() != Detail::SurfaceHealth::Ready) {
        return InvalidState("Render surface is not ready");
    }

    Base::Result<Aero::RenderFrameStatistics> statistics =
        Aero::RenderDevice::Impl::BeginSurfaceFrame(*impl->device, frame);
    if (!statistics) return statistics.GetStatus();

    Base::Result<void> rendered = impl->functions->render(
        impl->stateData, rendererToken, frame);
    if (!rendered) {
        Aero::RenderDevice::Impl::RecordSurfaceFailure(*impl->device);
        impl->RefreshHealth();
        return rendered.GetStatus();
    }

    Aero::RenderDevice::Impl::CompleteSurfaceFrame(
        *impl->device, frame, statistics.Value());
    return {};
}

namespace Detail {

Base::Result<Base::Ref<RenderSurface>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    return RenderSurface::Impl::Create(
        std::move(device), kind, allocator);
}

} // namespace Detail

#if defined(_WIN32)
Base::Result<Base::Ref<RenderSurface>> CreateD3D11EmbeddedSurface(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateD3D11EmbeddedDevice(options, allocator);
    if (!device) return device.GetStatus();
    return Detail::AdoptRenderSurface(
        std::move(device).Value(), RenderSurfaceKind::Embedded, allocator);
}

Base::Result<Base::Ref<RenderSurface>> CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateD3D11WindowDevice(options, allocator);
    if (!device) return device.GetStatus();
    return Detail::AdoptRenderSurface(
        std::move(device).Value(), RenderSurfaceKind::Window, allocator);
}
#endif

Base::Result<Base::Ref<RenderSurface>> CreateOpenGL33EmbeddedSurface(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateOpenGL33EmbeddedDevice(options, allocator);
    if (!device) return device.GetStatus();
    return Detail::AdoptRenderSurface(
        std::move(device).Value(), RenderSurfaceKind::Embedded, allocator);
}

Base::Result<Base::Ref<RenderSurface>> CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::Result<Base::Ref<Aero::RenderDevice>> device =
        CreateOpenGL33WindowDevice(options, allocator);
    if (!device) return device.GetStatus();
    return Detail::AdoptRenderSurface(
        std::move(device).Value(), RenderSurfaceKind::Window, allocator);
}

} // namespace Aero::Integration

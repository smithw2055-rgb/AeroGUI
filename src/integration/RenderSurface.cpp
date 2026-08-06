#include <Aero/Integration/D3D11.hpp>
#include <Aero/Integration/OpenGL33.hpp>

#include "integration/IntegrationPrivate.hpp"
#include "integration/private/RenderSurface.hpp"

#include <new>
#include <utility>

namespace Aero::Integration {

// Source-private backend constructors retained until native Device and Surface
// ownership are physically separated in the backend implementation files.
#if defined(_WIN32)
Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11EmbeddedDevice(
    const D3D11EmbeddedDeviceOptions& options,
    Base::IAllocator* allocator) noexcept;
Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11WindowDevice(
    const D3D11WindowDeviceOptions& options,
    Base::IAllocator* allocator) noexcept;
#endif
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33EmbeddedDevice(
    const OpenGL33EmbeddedDeviceOptions& options,
    Base::IAllocator* allocator) noexcept;
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33WindowDevice(
    const OpenGL33WindowDeviceOptions& options,
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
    switch (Aero::RenderDevice::Impl::SurfaceState(*impl_->device)) {
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
    return impl_ != nullptr && impl_->device
        ? Aero::RenderDevice::Impl::ResizeSurface(
              *impl_->device, width, height)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "Render surface is not initialized"));
}

void RenderSurface::NotifyLost() noexcept {
    if (impl_ != nullptr && impl_->device) {
        Aero::RenderDevice::Impl::NotifySurfaceLost(*impl_->device);
    }
}

Base::Result<void> RenderSurface::Restore() noexcept {
    return impl_ != nullptr && impl_->device
        ? Aero::RenderDevice::Impl::RestoreSurface(*impl_->device)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "Render surface is not initialized"));
}

Base::Result<Base::Ref<RenderSurface>> RenderSurface::Impl::Create(
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator) noexcept {
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render surface requires a render device");
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
    if (surface.impl_ == nullptr || !surface.impl_->device) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Render surface is not initialized");
    }
    Base::Status ready = Aero::RenderDevice::Impl::SurfaceStatus(
        *surface.impl_->device);
    if (!ready.IsOk()) return ready;
    return Aero::RenderDevice::Impl::Render(
        *surface.impl_->device, rendererToken, frame);
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

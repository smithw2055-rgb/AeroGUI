#pragma once

#include <Aero/Integration/RenderSurface.hpp>
#include "integration/private/RenderDevice.hpp"

namespace Aero::Integration::Detail {

// A RenderSurface is now only a typed owner/borrower of one native render
// target. Native targets use ordinary C++ virtual dispatch rather than a second
// state pointer plus function table parallel to RenderDevice.
class NativeRenderTarget {
public:
    virtual ~NativeRenderTarget() noexcept = default;

    virtual Base::Result<void> Render(
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept = 0;
    virtual Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface() noexcept = 0;
    virtual SurfaceHealth GetSurfaceHealth() const noexcept = 0;
};

} // namespace Aero::Integration::Detail

namespace Aero::Integration {

struct RenderSurface::Impl {
    Impl(
        Base::Ref<Aero::RenderDevice> selectedDevice,
        RenderSurfaceKind selectedKind,
        Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator),
          device(std::move(selectedDevice)),
          kind(selectedKind) {
        if (device) {
            target = Aero::RenderDevice::Impl::DefaultTarget(*device);
            RefreshHealth();
        }
    }

    ~Impl() noexcept { DestroyTarget(); }

    Base::IAllocator* allocator = nullptr;
    Base::Ref<Aero::RenderDevice> device;
    Detail::NativeRenderTarget* target = nullptr;
    RenderSurfaceKind kind = RenderSurfaceKind::Embedded;
    Detail::SurfaceHealth health = Detail::SurfaceHealth::Shutdown;
    bool ownsTarget = false;

    void DestroyTarget() noexcept {
        if (ownsTarget) delete target;
        target = nullptr;
        ownsTarget = false;
        health = Detail::SurfaceHealth::Shutdown;
    }

    void SetTarget(
        Detail::NativeRenderTarget* selectedTarget,
        bool owned) noexcept {
        DestroyTarget();
        target = selectedTarget;
        ownsTarget = owned;
        RefreshHealth();
    }

    Detail::SurfaceHealth RefreshHealth() noexcept {
        health = target != nullptr
            ? target->GetSurfaceHealth()
            : Detail::SurfaceHealth::Shutdown;
        return health;
    }

    static Base::Result<Base::Ref<RenderSurface>> Create(
        Base::Ref<Aero::RenderDevice> device,
        RenderSurfaceKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<Base::Ref<RenderSurface>> CreateOwned(
        Base::Ref<Aero::RenderDevice> device,
        Detail::NativeRenderTarget* target,
        RenderSurfaceKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<void> Render(
        RenderSurface& surface,
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept;
};

namespace Detail {

Base::Result<Base::Ref<RenderSurface>> AdoptRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<RenderSurface>> AdoptOwnedRenderSurface(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
    RenderSurfaceKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Detail
} // namespace Aero::Integration

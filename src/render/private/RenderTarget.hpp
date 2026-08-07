#pragma once

#include <Aero/RenderTarget.hpp>
#include "render/private/RenderDevice.hpp"

namespace Aero::Render::Detail {

// Private native target contract. It owns target-specific acquire/submit or
// presentation state; the installed SDK exposes only Aero::RenderTarget.
class NativeRenderTarget {
public:
    virtual ~NativeRenderTarget() noexcept = default;

    virtual Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept = 0;
    virtual Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface() noexcept = 0;
    virtual SurfaceHealth GetSurfaceHealth() const noexcept = 0;
};

} // namespace Aero::Render::Detail

namespace Aero {

struct RenderTarget::Impl {
    Impl(
        Base::Ref<Aero::RenderDevice> selectedDevice,
        RenderTargetKind selectedKind,
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
    ::Aero::Render::Detail::NativeRenderTarget* target = nullptr;
    RenderTargetKind kind = RenderTargetKind::Embedded;
    ::Aero::Render::Detail::SurfaceHealth health =
        ::Aero::Render::Detail::SurfaceHealth::Shutdown;
    bool ownsTarget = false;

    void DestroyTarget() noexcept {
        if (ownsTarget) delete target;
        target = nullptr;
        ownsTarget = false;
        health = ::Aero::Render::Detail::SurfaceHealth::Shutdown;
    }

    void SetTarget(
        ::Aero::Render::Detail::NativeRenderTarget* selectedTarget,
        bool owned) noexcept {
        DestroyTarget();
        target = selectedTarget;
        ownsTarget = owned;
        RefreshHealth();
    }

    ::Aero::Render::Detail::SurfaceHealth RefreshHealth() noexcept {
        health = target != nullptr
            ? target->GetSurfaceHealth()
            : ::Aero::Render::Detail::SurfaceHealth::Shutdown;
        return health;
    }

    static Base::Result<Base::Ref<RenderTarget>> Create(
        Base::Ref<Aero::RenderDevice> device,
        Aero::RenderTargetKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<Base::Ref<RenderTarget>> CreateOwned(
        Base::Ref<Aero::RenderDevice> device,
        ::Aero::Render::Detail::NativeRenderTarget* target,
        Aero::RenderTargetKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;

    static Base::Result<void> Render(
        RenderTarget& target,
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
};

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    NativeRenderTarget* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

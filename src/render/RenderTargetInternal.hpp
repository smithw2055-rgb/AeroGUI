#pragma once

#include <Aero/Gui/RenderTarget.hpp>
#include "render/RenderDeviceInternal.hpp"

namespace Aero {

// Backend target state derives directly from the public target's source-private
// Impl. This removes the former Impl -> NativeRenderTarget delegation object.
struct RenderTarget::Impl {
    explicit Impl(
        RenderTargetKind selectedKind = RenderTargetKind::Embedded) noexcept
        : kind(selectedKind) {}
    virtual ~Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    virtual Base::Result<void> Render(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept = 0;
    virtual Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface() noexcept = 0;
    virtual ::Aero::Render::Detail::SurfaceHealth
        GetSurfaceHealth() const noexcept = 0;

    // Desktop presentation is driven by App::Detail::RenderContext. Embedded
    // targets keep the explicit IRenderer::Render(target) one-call contract.
    // A window target therefore defers its native present while frameOpen is
    // true and completes it through PresentFrame().
    virtual Base::Result<void> PresentFrame() noexcept;
    virtual void DiscardFrame() noexcept;

    RenderTargetKind kind = RenderTargetKind::Embedded;
    bool frameOpen = false;
    bool frameRendered = false;
    bool frameEnded = false;

    static Base::Result<Base::Ref<RenderTarget>> Create(
        Base::Ref<Aero::RenderDevice> device,
        Impl* implementation,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<void> Render(
        RenderTarget& target,
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    static Base::Result<void> BeginFrame(RenderTarget& target) noexcept;
    static Base::Result<void> EndFrame(RenderTarget& target) noexcept;
    static Base::Result<void> Present(RenderTarget& target) noexcept;
    static void CancelFrame(RenderTarget& target) noexcept;
};

} // namespace Aero

namespace Aero::Render::Detail {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Impl* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

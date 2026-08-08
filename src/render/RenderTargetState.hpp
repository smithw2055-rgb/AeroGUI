#pragma once

#include <Aero/Render/RenderTarget.hpp>
#include "gui/ViewRenderer.hpp"
#include "render/RenderDeviceState.hpp"

namespace Aero {

// Backend target state derives directly from the public target's source-private
// Access. This removes the former Access -> NativeRenderTarget delegation object.
struct RenderTarget::Access {
    explicit Access(
        RenderTargetKind selectedKind = RenderTargetKind::Embedded) noexcept
        : kind(selectedKind) {}
    virtual ~Access() noexcept = default;

    Access(const Access&) = delete;
    Access& operator=(const Access&) = delete;

    virtual Base::Result<void> Render(
        ::Aero::ViewRenderer& renderer,
        const ::Aero::Render::RenderFrame& frame) noexcept = 0;
    virtual Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface() noexcept = 0;
    virtual ::Aero::Render::SurfaceHealth
        GetSurfaceHealth() const noexcept = 0;

    RenderTargetKind kind = RenderTargetKind::Embedded;

    static Base::Result<Base::Ref<RenderTarget>> Create(
        Base::Ref<Aero::RenderDevice> device,
        Access* implementation,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<void> Render(
        RenderTarget& target,
        ::Aero::ViewRenderer& renderer,
        const ::Aero::Render::RenderFrame& frame) noexcept;
};

} // namespace Aero

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Access* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render

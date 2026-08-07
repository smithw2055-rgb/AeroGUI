#pragma once

#include <Aero/RenderTarget.hpp>
#include "render/private/RenderDevice.hpp"

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

    RenderTargetKind kind = RenderTargetKind::Embedded;

    static Base::Result<Base::Ref<RenderTarget>> CreateBorrowed(
        Base::Ref<Aero::RenderDevice> device,
        Impl* implementation,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<Base::Ref<RenderTarget>> CreateOwned(
        Base::Ref<Aero::RenderDevice> device,
        Impl* implementation,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<void> RenderTargetFrame(
        RenderTarget& target,
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
};

} // namespace Aero

namespace Aero::Render::Detail {

// Compatibility spelling for existing backend implementation declarations.
// It is an alias, not an additional object/lifetime layer.
using NativeRenderTarget = ::Aero::RenderTarget::Impl;

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderTarget>> AdoptOwnedRenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    Aero::RenderTarget::Impl* target,
    Aero::RenderTargetKind kind,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

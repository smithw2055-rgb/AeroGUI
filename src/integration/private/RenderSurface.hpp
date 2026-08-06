#pragma once

#include <Aero/Integration/RenderSurface.hpp>
#include "integration/private/RenderDevice.hpp"

namespace Aero::Integration {

struct RenderSurface::Impl {
    Impl(
        Base::Ref<Aero::RenderDevice> selectedDevice,
        RenderSurfaceKind selectedKind,
        Base::IAllocator& selectedAllocator) noexcept
        : allocator(&selectedAllocator),
          device(std::move(selectedDevice)),
          kind(selectedKind) {}

    Base::IAllocator* allocator = nullptr;
    Base::Ref<Aero::RenderDevice> device;
    RenderSurfaceKind kind = RenderSurfaceKind::Embedded;

    static Base::Result<Base::Ref<RenderSurface>> Create(
        Base::Ref<Aero::RenderDevice> device,
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

} // namespace Detail
} // namespace Aero::Integration

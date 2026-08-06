#include <Aero/Render/D3D11.hpp>
#include "integration/BackendApi.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateD3D11Device(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateD3D11EmbeddedSurface(
        std::move(device), options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11WindowSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateD3D11WindowSurface(
        std::move(device), options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11EmbeddedSurface(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateD3D11EmbeddedSurface(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return Integration::CreateD3D11WindowSurface(options, allocator);
}

} // namespace Aero::Render

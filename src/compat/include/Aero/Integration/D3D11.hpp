#pragma once
#include <Aero/Render/D3D11.hpp>

namespace Aero::Integration {
using D3D11StatePreservationPolicy =
    ::Aero::Render::D3D11StatePreservationPolicy;
using D3D11DeviceOptions = ::Aero::Render::D3D11DeviceOptions;
using D3D11EmbeddedTarget = ::Aero::Render::D3D11EmbeddedTarget;
using D3D11TargetCallback = ::Aero::Render::D3D11TargetCallback;
using D3D11EmbeddedSurfaceOptions =
    ::Aero::Render::D3D11EmbeddedSurfaceOptions;
using D3D11WindowSurfaceOptions =
    ::Aero::Render::D3D11WindowSurfaceOptions;

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11WindowSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11EmbeddedSurface(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderSurface>> CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
} // namespace Aero::Integration

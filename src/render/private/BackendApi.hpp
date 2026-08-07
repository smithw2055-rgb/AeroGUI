#pragma once

#include <Aero/Render/D3D11.hpp>
#include <Aero/Render/OpenGL33.hpp>
#include "render/private/RenderSurface.hpp"

namespace Aero::Render::Detail {

using PresentMode = ::Aero::PresentMode;
using RenderSurfaceKind = ::Aero::RenderTargetKind;
using RenderSurfaceState = ::Aero::RenderTargetState;
using RenderSurface = ::Aero::RenderTarget;

using D3D11StatePreservationPolicy =
    ::Aero::Render::D3D11StatePreservationPolicy;
using D3D11DeviceOptions = ::Aero::Render::D3D11DeviceOptions;
using D3D11EmbeddedTarget = ::Aero::Render::D3D11EmbeddedTarget;
using D3D11TargetCallback = ::Aero::Render::D3D11TargetCallback;
using D3D11EmbeddedSurfaceOptions =
    ::Aero::Render::D3D11RenderTargetOptions;
using D3D11WindowSurfaceOptions =
    ::Aero::Render::D3D11WindowSurfaceOptions;

using OpenGL33StatePreservationPolicy =
    ::Aero::Render::OpenGL33StatePreservationPolicy;
using OpenGL33ProcAddress = ::Aero::Render::OpenGL33ProcAddress;
using OpenGL33ProcResolver = ::Aero::Render::OpenGL33ProcResolver;
using OpenGL33MakeCurrent = ::Aero::Render::OpenGL33MakeCurrent;
using OpenGL33IsCurrent = ::Aero::Render::OpenGL33IsCurrent;
using OpenGL33ContextGeneration =
    ::Aero::Render::OpenGL33ContextGeneration;
using OpenGL33DeviceOptions = ::Aero::Render::OpenGL33DeviceOptions;
using OpenGL33EmbeddedTarget = ::Aero::Render::OpenGL33EmbeddedTarget;
using OpenGL33TargetCallback = ::Aero::Render::OpenGL33TargetCallback;
using OpenGL33EmbeddedSurfaceOptions =
    ::Aero::Render::OpenGL33RenderTargetOptions;
using OpenGL33WindowSurfaceOptions =
    ::Aero::Render::OpenGL33WindowSurfaceOptions;

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11WindowSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedSurface(
    const D3D11EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>>
CreateOpenGL33EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>>
CreateOpenGL33EmbeddedSurface(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>>
CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>>
CreateOpenGL33WindowDevice(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

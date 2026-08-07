#pragma once

#include <Aero/Platform/NativeWindow.hpp>
#define AERO_RENDER_BACKEND_IMPLEMENTATION 1
#include <Aero/Render/D3D11.hpp>
#include <Aero/Render/OpenGL33.hpp>
#undef AERO_RENDER_BACKEND_IMPLEMENTATION
#include "render/Surface.hpp"
#include "render/private/RenderTarget.hpp"

namespace Aero::Render::Detail {

using PresentMode = ::Aero::Graphics::PresentMode;

using D3D11StatePreservationPolicy =
    ::Aero::Render::D3D11StatePreservationPolicy;
using D3D11DeviceOptions = ::Aero::Render::D3D11DeviceOptions;
using D3D11EmbeddedTarget = ::Aero::Render::D3D11EmbeddedTarget;
using D3D11TargetCallback = ::Aero::Render::D3D11TargetCallback;

struct D3D11EmbeddedTargetOptions {
    D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
};

struct D3D11WindowTargetOptions {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    PresentMode presentMode = PresentMode::Fifo;
};

using OpenGL33StatePreservationPolicy =
    ::Aero::Render::OpenGL33StatePreservationPolicy;
using OpenGL33ProcAddress = ::Aero::Render::OpenGL33ProcAddress;
using OpenGL33ProcResolver = ::Aero::Render::OpenGL33ProcResolver;
using OpenGL33MakeCurrent = ::Aero::Render::OpenGL33MakeCurrent;
using OpenGL33IsCurrent = ::Aero::Render::OpenGL33IsCurrent;
using OpenGL33ContextGeneration = ::Aero::Render::OpenGL33ContextGeneration;
using OpenGL33DeviceOptions = ::Aero::Render::OpenGL33DeviceOptions;
using OpenGL33EmbeddedTarget = ::Aero::Render::OpenGL33EmbeddedTarget;
using OpenGL33TargetCallback = ::Aero::Render::OpenGL33TargetCallback;

struct OpenGL33EmbeddedTargetOptions {
    OpenGL33TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    void* targetContext = nullptr;
};

struct OpenGL33WindowTargetOptions {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    PresentMode presentMode = PresentMode::Fifo;
    bool enableDebugContext = false;
};

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11WindowTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33WindowDevice(
    const OpenGL33WindowTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render::Detail

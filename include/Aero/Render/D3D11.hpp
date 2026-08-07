#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderTarget.hpp>
#include <Aero/Platform/NativeWindow.hpp>

#include <cstdint>

namespace Aero::Render {

enum class D3D11StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

struct D3D11DeviceOptions {
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    D3D11StatePreservationPolicy statePolicy =
        D3D11StatePreservationPolicy::HostResetsState;
    bool useWarp = false;
    bool allowWarpFallback = true;
    bool enableDebugLayer = false;
};

struct D3D11EmbeddedTarget  {
    std::uintptr_t texture2D = 0U;
    std::uintptr_t renderTargetView = 0U;
    std::uintptr_t depthStencilView = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t stableId = 0U;
};

using D3D11TargetCallback = Base::Status (*)(
    void* context,
    D3D11EmbeddedTarget* target) noexcept;

struct D3D11RenderTargetOptions  {
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    D3D11StatePreservationPolicy statePolicy =
        D3D11StatePreservationPolicy::HostResetsState;
};

// Window-surface construction remains available for the optional App product.
// Embedded hosts should use CreateD3D11RenderTarget() instead.
struct D3D11WindowSurfaceOptions  {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    PresentMode presentMode = PresentMode::Fifo;
    bool useWarp = false;
    bool allowWarpFallback = true;
    bool enableDebugLayer = false;
};

AERO_API Base::Result<Base::Ref<Aero::RenderDevice>>
CreateD3D11Device(
    const D3D11DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<Aero::RenderTarget>>
CreateD3D11RenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11RenderTargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<Aero::RenderTarget>>
CreateD3D11WindowSurface(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<Aero::RenderTarget>>
CreateD3D11WindowSurface(
    const D3D11WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Render

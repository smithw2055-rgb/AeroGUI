#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderDevice.hpp>
#include <Aero/Integration/NativeWindow.hpp>

#include <cstdint>

namespace Aero::Integration {

enum class D3D11StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
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

struct D3D11EmbeddedDeviceOptions  {
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    D3D11StatePreservationPolicy statePolicy =
        D3D11StatePreservationPolicy::HostResetsState;
};

struct D3D11WindowDeviceOptions  {
    NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    Aero::RenderPresentMode presentMode = Aero::RenderPresentMode::Fifo;
    bool useWarp = false;
    bool allowWarpFallback = true;
    bool enableDebugLayer = false;
};

AERO_API Base::Result<Base::Ref<Aero::RenderDevice>>
CreateD3D11EmbeddedDevice(
    const D3D11EmbeddedDeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<Aero::RenderDevice>>
CreateD3D11WindowDevice(
    const D3D11WindowDeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration

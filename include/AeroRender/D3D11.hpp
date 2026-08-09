#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderTarget.hpp>

#include <cstdint>

namespace Aero::Render::D3D11 {

enum class StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

struct DeviceOptions {
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    StatePreservationPolicy statePolicy =
        StatePreservationPolicy::HostResetsState;
    bool useWarp = false;
    bool allowWarpFallback = true;
    bool enableDebugLayer = false;
};

struct EmbeddedTarget {
    std::uintptr_t texture2D = 0U;
    std::uintptr_t renderTargetView = 0U;
    std::uintptr_t depthStencilView = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t stableId = 0U;
};

using TargetCallback = Base::Status (*)(
    void* context,
    EmbeddedTarget* target) noexcept;

// Target options describe only how to acquire the host-owned target. Device and
// immediate-context ownership belongs to the explicitly supplied RenderDevice.
struct TargetOptions {
    TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    // Desktop hosts normally clear a newly acquired swap-chain buffer. Embedded
    // hosts keep the existing contents unless they opt in explicitly.
    bool clearBeforeRender = false;
};

AERO_RENDER_D3D11_API Result<Ref<Aero::RenderDevice>>
CreateDevice(
    const DeviceOptions& options = {},
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_RENDER_D3D11_API Result<Ref<Aero::RenderTarget>>
CreateTarget(
    Ref<Aero::RenderDevice> device,
    const TargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
} // namespace Aero::Render::D3D11

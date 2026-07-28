#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>
#include <Aero/Platform/Window.hpp>

#include <cstdint>

namespace Aero::Integration {

enum class D3D11StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

struct D3D11EmbeddedTarget final {
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

struct D3D11EmbeddedEndpointOptions final {
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    D3D11TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    D3D11StatePreservationPolicy statePolicy =
        D3D11StatePreservationPolicy::HostResetsState;
    RenderSubmissionMode submissionMode =
        RenderSubmissionMode::Immediate;
};

struct D3D11WindowEndpointOptions final {
    Platform::NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    RenderPresentMode presentMode = RenderPresentMode::Fifo;
    RenderSubmissionMode submissionMode =
        RenderSubmissionMode::Immediate;
    bool useWarp = false;
    bool allowWarpFallback = true;
    bool enableDebugLayer = false;
};

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateD3D11EmbeddedEndpoint(
    const D3D11EmbeddedEndpointOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateD3D11WindowEndpoint(
    const D3D11WindowEndpointOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration

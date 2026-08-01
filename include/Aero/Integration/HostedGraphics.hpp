#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>

#include <cstdint>

namespace Aero::Integration {

inline constexpr std::uint32_t HostedGraphicsAbiVersion = 1U;

enum class HostedGraphicsResult : std::uint32_t {
    Success = 0U,
    InvalidArgument,
    Unsupported,
    OutOfMemory,
    SurfaceLost,
    DeviceLost,
    Failed
};

enum HostedGraphicsCapability : std::uint64_t {
    HostedGraphicsCapabilityNone = 0U,
    HostedGraphicsCapabilityEmbeddedTarget =
        UINT64_C(1) << 1U,
    HostedGraphicsCapabilityWindowSurface =
        UINT64_C(1) << 2U,
    HostedGraphicsCapabilityFences =
        UINT64_C(1) << 3U
};

using HostedGraphicsCapabilityFlags = std::uint64_t;

enum class HostedGraphicsResourceKind : std::uint8_t {
    Invalid = 0U,
    Buffer,
    Texture,
    Sampler,
    Pipeline
};

struct HostedGraphicsResourceHandle final {
    std::uint64_t id = 0U;
    std::uint32_t generation = 0U;
    HostedGraphicsResourceKind kind =
        HostedGraphicsResourceKind::Invalid;
};

struct HostedGraphicsResourceDescriptor final {
    std::uint32_t structSize =
        sizeof(HostedGraphicsResourceDescriptor);
    HostedGraphicsResourceKind kind =
        HostedGraphicsResourceKind::Invalid;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t byteSize = 0U;
    std::uint32_t format = 0U;
    std::uint32_t usage = 0U;
    const void* initialData = nullptr;
    std::uint32_t initialDataSize = 0U;
};

enum class HostedGraphicsCommandKind : std::uint8_t {
    PushClip = 0U,
    PopClip,
    PushOpacity,
    PopOpacity,
    PushTransform,
    PopTransform,
    FillRect,
    FillRoundedRect,
    StrokeRect,
    DrawImage,
    DrawMesh,
    DrawGlyphRun
};

// Backend-neutral, read-only command ABI. Values contain only copied scalar
// data and generation-safe resource tokens; no UI object crosses this seam.
struct HostedGraphicsCommand final {
    HostedGraphicsCommandKind kind =
        HostedGraphicsCommandKind::FillRect;
    float rect[4]{};
    float transform[6]{};
    float color[4]{};
    float sourceUv[4]{};
    std::uint64_t resourceId = 0U;
    std::uint32_t resourceGeneration = 0U;
    float scalar = 0.0F;
};

struct HostedGraphicsCommandListView final {
    std::uint32_t structSize =
        sizeof(HostedGraphicsCommandListView);
    std::uint32_t abiVersion = HostedGraphicsAbiVersion;
    const HostedGraphicsCommand* commands = nullptr;
    std::uint32_t commandCount = 0U;
    std::uint64_t frameVersion = 0U;
};

struct HostedGraphicsTarget final {
    std::uintptr_t colorTarget = 0U;
    std::uintptr_t depthStencilTarget = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t stableId = 0U;
    bool defaultFramebuffer = false;
};

struct HostedGraphicsCallbacks final {
    std::uint32_t structSize = sizeof(HostedGraphicsCallbacks);
    std::uint32_t abiVersion = HostedGraphicsAbiVersion;
    void* context = nullptr;
    HostedGraphicsCapabilityFlags capabilities =
        HostedGraphicsCapabilityNone;

    HostedGraphicsResult (*createResource)(
        void*,
        HostedGraphicsResourceHandle,
        const HostedGraphicsResourceDescriptor*) noexcept = nullptr;
    void (*destroyResource)(
        void*,
        HostedGraphicsResourceHandle) noexcept = nullptr;
    HostedGraphicsResult (*configureResource)(
        void*,
        HostedGraphicsResourceHandle,
        const HostedGraphicsResourceDescriptor*) noexcept = nullptr;
    HostedGraphicsResult (*submit)(
        void*,
        const HostedGraphicsTarget*,
        const HostedGraphicsCommandListView*,
        std::uint64_t signalFence) noexcept = nullptr;
    HostedGraphicsResult (*waitFence)(
        void*,
        std::uint64_t fence,
        std::uint32_t timeoutMilliseconds) noexcept = nullptr;
    bool (*isDeviceLost)(void*) noexcept = nullptr;
    void (*notifyDeviceLost)(void*) noexcept = nullptr;
    HostedGraphicsResult (*restoreDevice)(void*) noexcept = nullptr;

    HostedGraphicsResult (*acquireTarget)(
        void*,
        HostedGraphicsTarget*) noexcept = nullptr;
    HostedGraphicsResult (*resizeSurface)(
        void*,
        std::uint32_t,
        std::uint32_t) noexcept = nullptr;
    HostedGraphicsResult (*present)(
        void*,
        std::uint64_t signalFence) noexcept = nullptr;
    void (*notifySurfaceLost)(void*) noexcept = nullptr;
    HostedGraphicsResult (*restoreSurface)(void*) noexcept = nullptr;
};

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateHostedEmbeddedEndpoint(
    const HostedGraphicsCallbacks& callbacks,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateHostedWindowEndpoint(
    const HostedGraphicsCallbacks& callbacks,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration

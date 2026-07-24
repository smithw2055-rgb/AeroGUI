#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Rhi/Graphics.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Rhi {

constexpr std::uint32_t SurfaceAbiVersion = 1U;
constexpr std::uint32_t HostedGraphicsAbiVersion = 2U;

enum class SurfaceKind : std::uint8_t {
    Invalid = 0U,
    Headless,
    D3D11Window,
    WglWindow,
    GlxWindow,
    EglWindow,
    WebGL2Canvas,
    ExternalRenderTarget
};

using SurfaceKindFlags = std::uint32_t;

constexpr SurfaceKindFlags SurfaceKindBit(
    SurfaceKind kind) noexcept {
    return kind == SurfaceKind::Invalid
        ? 0U
        : (UINT32_C(1) << static_cast<std::uint32_t>(kind));
}

constexpr bool SupportsSurfaceKind(
    SurfaceKindFlags available,
    SurfaceKind kind) noexcept {
    return (available & SurfaceKindBit(kind)) != 0U;
}

enum class SurfaceOwnership : std::uint8_t {
    Borrowed = 0U,
    Owned
};

enum class SurfaceState : std::uint8_t {
    Uninitialized = 0U,
    Ready,
    Lost,
    Destroyed
};

enum class PresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

struct D3D11SurfaceNative final {
    std::uintptr_t window = 0U;
    std::uintptr_t device = 0U;
    std::uintptr_t immediateContext = 0U;
    std::uintptr_t swapChain = 0U;
};

struct WglSurfaceNative final {
    std::uintptr_t window = 0U;
    std::uintptr_t deviceContext = 0U;
    std::uintptr_t renderContext = 0U;
};

struct GlxSurfaceNative final {
    std::uintptr_t display = 0U;
    std::uintptr_t drawable = 0U;
    std::uintptr_t context = 0U;
    std::int32_t screen = 0;
};

struct EglSurfaceNative final {
    std::uintptr_t display = 0U;
    std::uintptr_t surface = 0U;
    std::uintptr_t context = 0U;
};

struct WebGL2SurfaceNative final {
    std::uint32_t contextHandle = 0U;
    std::uint64_t canvasId = 0U;
    bool offscreenCanvas = false;
};

struct ExternalSurfaceNative final {
    std::uintptr_t colorTarget = 0U;
    std::uintptr_t depthStencilTarget = 0U;
    bool defaultFramebuffer = false;
};

struct NativeSurfaceDescriptor final {
    std::uint32_t abiVersion = SurfaceAbiVersion;
    SurfaceKind kind = SurfaceKind::Invalid;
    SurfaceOwnership ownership = SurfaceOwnership::Borrowed;
    PresentMode presentMode = PresentMode::Fifo;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    std::uint64_t stableId = 0U;
    D3D11SurfaceNative d3d11;
    WglSurfaceNative wgl;
    GlxSurfaceNative glx;
    EglSurfaceNative egl;
    WebGL2SurfaceNative webgl2;
    ExternalSurfaceNative external;
};

struct SurfaceCapabilities final {
    std::uint32_t abiVersion = SurfaceAbiVersion;
    SurfaceKindFlags supportedKinds = 0U;
    std::uint32_t maxWidth = 16384U;
    std::uint32_t maxHeight = 16384U;
    bool supportsResize = false;
    bool supportsPresent = false;
    bool supportsContextLossRecovery = false;
    bool supportsExternalRenderTargets = false;
};

struct ExternalRenderTargetDescriptor final {
    std::uintptr_t colorTarget = 0U;
    std::uintptr_t depthStencilTarget = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    bool defaultFramebuffer = false;
    std::uint64_t stableId = 0U;
};

struct SurfaceFrameTarget final {
    ResourceHandle color;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    GraphicsTextureFormat colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    GraphicsTextureFormat depthStencilFormat =
        GraphicsTextureFormat::Depth24Stencil8;
    std::uint8_t sampleCount = 1U;
    bool defaultFramebuffer = false;
    std::uint64_t stableId = 0U;
};

struct SurfaceFrame final {
    std::uint64_t surfaceGeneration = 0U;
    std::uint64_t frameSerial = 0U;
    SurfaceFrameTarget target;
};

AERO_API Base::Result<void> ValidateNativeSurfaceDescriptor(
    const NativeSurfaceDescriptor& descriptor,
    const SurfaceCapabilities& capabilities) noexcept;

AERO_API Base::Result<void>
ValidateExternalRenderTargetDescriptor(
    const ExternalRenderTargetDescriptor& descriptor) noexcept;

class AERO_API ISurfaceBackend {
public:
    virtual ~ISurfaceBackend() = default;

    virtual SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept = 0;
    virtual Base::Result<void> CreateSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept = 0;
    virtual void DestroySurface() noexcept = 0;
    virtual Base::Result<void> ResizeSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual Base::Result<ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t frameSerial) noexcept = 0;
    virtual Base::Result<void> PresentSurface(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept = 0;
    virtual void DiscardSurfaceFrame(std::uint64_t frameSerial) noexcept = 0;
    virtual void NotifySurfaceLost() noexcept = 0;
    virtual Base::Result<void> RestoreSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept = 0;
    virtual bool IsSurfaceLost() const noexcept = 0;
};

class AERO_API SurfaceSession final {
public:
    SurfaceSession(
        RhiDevice& device,
        ISurfaceBackend& backend) noexcept
        : device_(&device), backend_(&backend) {}
    ~SurfaceSession() noexcept;

    SurfaceSession(const SurfaceSession&) = delete;
    SurfaceSession& operator=(const SurfaceSession&) = delete;

    Base::Result<void> Initialize(
        const NativeSurfaceDescriptor& descriptor) noexcept;
    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<SurfaceFrame> AcquireFrame() noexcept;
    Base::Result<void> Present(
        SurfaceFrame& frame,
        FenceValue signalFence) noexcept;
    Base::Result<void> DiscardFrame(
        SurfaceFrame& frame) noexcept;
    Base::Result<void> NotifyContextLost() noexcept;
    Base::Result<void> Restore(
        const NativeSurfaceDescriptor& descriptor) noexcept;
    void Shutdown() noexcept;

    SurfaceState State() const noexcept { return state_; }
    std::uint64_t Generation() const noexcept {
        return generation_;
    }
    bool HasFrameInFlight() const noexcept {
        return activeFrameSerial_ != 0U;
    }
    const NativeSurfaceDescriptor& Descriptor() const noexcept {
        return descriptor_;
    }
    const SurfaceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }
    bool IsCurrentFrame(
        const SurfaceFrame& frame) const noexcept;

private:
    RhiDevice* device_ = nullptr;
    ISurfaceBackend* backend_ = nullptr;
    SurfaceCapabilities capabilities_;
    NativeSurfaceDescriptor descriptor_;
    SurfaceState state_ = SurfaceState::Uninitialized;
    std::uint64_t generation_ = 0U;
    std::uint64_t nextFrameSerial_ = 1U;
    std::uint64_t activeFrameSerial_ = 0U;
    ResourceHandle activeTarget_;

    Base::Result<void> VerifyReady() noexcept;
    Base::Result<void> AdvanceGeneration() noexcept;
    Base::Result<void> RetireActiveTarget(
        FenceValue retireAfter) noexcept;
    void ClearFrame(SurfaceFrame& frame) noexcept;
};

struct HostedGraphicsApi final {
    std::uint32_t structSize = 0U;
    std::uint32_t abiVersion = HostedGraphicsAbiVersion;
    void* context = nullptr;
    GraphicsBackendKind kind = GraphicsBackendKind::Invalid;

    DeviceCapabilities (*deviceCapabilities)(void*) noexcept = nullptr;
    GraphicsCapabilities (*graphicsCapabilities)(void*) noexcept = nullptr;
    SurfaceCapabilities (*surfaceCapabilities)(void*) noexcept = nullptr;

    Base::Result<void> (*createResource)(
        void*, ResourceHandle, const ResourceDescriptor&) noexcept = nullptr;
    void (*destroyResource)(void*, ResourceHandle) noexcept = nullptr;
    Base::Result<void> (*configureTexture)(
        void*, ResourceHandle, const TextureResourceDescriptor&) noexcept = nullptr;
    Base::Result<void> (*configureSampler)(
        void*, ResourceHandle, const SamplerDescriptor&) noexcept = nullptr;
    Base::Result<void> (*configurePipeline)(
        void*, ResourceHandle, const PipelineDescriptor&) noexcept = nullptr;
    Base::Result<void> (*importRenderTarget)(
        void*, ResourceHandle, const ExternalRenderTargetDescriptor&) noexcept = nullptr;
    Base::Result<void> (*submit)(
        void*, const GraphicsCommandBuffer&, FenceValue) noexcept = nullptr;
    FenceValue (*completedFence)(void*) noexcept = nullptr;
    bool (*isDeviceLost)(void*) noexcept = nullptr;

    Base::Result<void> (*createSurface)(
        void*, const NativeSurfaceDescriptor&) noexcept = nullptr;
    void (*destroySurface)(void*) noexcept = nullptr;
    Base::Result<void> (*resizeSurface)(
        void*, std::uint32_t, std::uint32_t) noexcept = nullptr;
    Base::Result<ExternalRenderTargetDescriptor> (*acquireSurfaceTarget)(
        void*, std::uint64_t) noexcept = nullptr;
    Base::Result<void> (*presentSurface)(
        void*, std::uint64_t, FenceValue) noexcept = nullptr;
    void (*discardSurfaceFrame)(void*, std::uint64_t) noexcept = nullptr;
    void (*notifySurfaceLost)(void*) noexcept = nullptr;
    Base::Result<void> (*restoreSurface)(
        void*, const NativeSurfaceDescriptor&) noexcept = nullptr;
    bool (*isSurfaceLost)(void*) noexcept = nullptr;
};

class AERO_API HostedGraphicsBackend final
    : public IGraphicsBackend,
      public ISurfaceBackend {
public:
    explicit HostedGraphicsBackend(const HostedGraphicsApi& api) noexcept
        : api_(api) {}

    bool IsValid() const noexcept;

    DeviceCapabilities Capabilities() const noexcept override;
    GraphicsBackendKind Kind() const noexcept override {
        return api_.kind;
    }
    GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept override;
    SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept override;

    Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override;
    void DestroyResource(ResourceHandle handle) noexcept override;
    Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept override;
    Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept override;
    Base::Result<void> ImportRenderTarget(
        ResourceHandle handle,
        const ExternalRenderTargetDescriptor& descriptor) noexcept override;
    Base::Result<void> Submit(
        const GraphicsCommandBuffer& commands,
        FenceValue signalFence) noexcept override;
    FenceValue LastSubmittedFence() const noexcept override {
        return lastSubmittedFence_;
    }
    FenceValue CompletedFence() const noexcept override;
    bool IsDeviceLost() const noexcept override;

    Base::Result<void> CreateSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    void DestroySurface() noexcept override;
    Base::Result<void> ResizeSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept override;
    Base::Result<ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t frameSerial) noexcept override;
    Base::Result<void> PresentSurface(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept override;
    void DiscardSurfaceFrame(std::uint64_t frameSerial) noexcept override;
    void NotifySurfaceLost() noexcept override;
    Base::Result<void> RestoreSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    bool IsSurfaceLost() const noexcept override;

private:
    HostedGraphicsApi api_;
    FenceValue lastSubmittedFence_ = 0U;

    Base::Result<void> VerifyApi() const noexcept;
    Base::Result<void> VerifySubmission(
        FenceValue signalFence) const noexcept;
};

} // namespace Aero::Rhi

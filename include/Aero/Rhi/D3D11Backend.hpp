#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Core/Rendering.hpp>
#include <Aero/Rhi/Graphics.hpp>
#include <Aero/Rhi/Surface.hpp>

#include <cstdint>

namespace Aero::Rhi {

enum class D3D11DeviceMode : std::uint8_t {
    Hardware = 0U,
    Warp,
    Borrowed
};

// Defines ownership of an immediate context after an AeroGUI submission.
// PreserveRequiredState captures the state that this backend changes and
// restores it before returning. HostResetsState avoids that query/restore cost;
// the host must then establish its own pipeline state before rendering again.
enum class D3D11StatePolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

struct D3D11BackendOptions final {
    D3D11DeviceMode deviceMode = D3D11DeviceMode::Hardware;
    D3D11StatePolicy statePolicy = D3D11StatePolicy::HostResetsState;
    bool enableDebugLayer = false;
    bool allowWarpFallback = true;
    std::uintptr_t borrowedDevice = 0U;
    std::uintptr_t borrowedImmediateContext = 0U;
};

struct D3D11ExternalRenderTargetDescriptor final {
    std::uintptr_t texture2D = 0U;
    std::uintptr_t renderTargetView = 0U;
    std::uintptr_t depthStencilView = 0U;
    TextureResourceDescriptor texture;
    std::uint64_t stableId = 0U;
};

class AERO_API D3D11GraphicsBackend final : public IGraphicsBackend {
public:
    explicit D3D11GraphicsBackend(
        const D3D11BackendOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11GraphicsBackend() noexcept override;

    D3D11GraphicsBackend(const D3D11GraphicsBackend&) = delete;
    D3D11GraphicsBackend& operator=(const D3D11GraphicsBackend&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    AERO_NODISCARD bool IsInitialized() const noexcept;
    AERO_NODISCARD std::uintptr_t NativeDevice() const noexcept;
    AERO_NODISCARD std::uintptr_t NativeImmediateContext() const noexcept;
    AERO_NODISCARD std::uint32_t NativeFeatureLevel() const noexcept;
    AERO_NODISCARD std::uint32_t LiveResourceCount() const noexcept;
    AERO_NODISCARD FenceValue LastSubmittedFence() const noexcept override;

    AERO_NODISCARD Base::Result<void> ImportExternalRenderTarget(
        ResourceHandle handle,
        const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept;

    AERO_NODISCARD Base::Result<void> ReadbackTexture(
        ResourceHandle handle,
        Base::Span<std::uint8_t> destination,
        std::uint32_t destinationRowPitch) noexcept;
    AERO_NODISCARD Base::Result<std::uint64_t> ReadbackTextureChecksum(
        ResourceHandle handle) noexcept;
    AERO_NODISCARD Base::Result<void> WaitForFence(
        FenceValue fence,
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

    AERO_NODISCARD DeviceCapabilities Capabilities() const noexcept override;
    AERO_NODISCARD GraphicsBackendKind Kind() const noexcept override {
        return GraphicsBackendKind::D3D11;
    }
    AERO_NODISCARD GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept override;

    AERO_NODISCARD Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override;
    void DestroyResource(ResourceHandle handle) noexcept override;
    AERO_NODISCARD Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept override;
    AERO_NODISCARD Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept override;
    AERO_NODISCARD Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept override;
    AERO_NODISCARD Base::Result<void> Submit(
        const CommandBuffer& commands,
        FenceValue signalFence) noexcept override;
    AERO_NODISCARD Base::Result<void> SubmitGraphics(
        const GraphicsCommandBuffer& commands,
        FenceValue signalFence) noexcept override;
    AERO_NODISCARD FenceValue CompletedFence() const noexcept override;
    AERO_NODISCARD bool IsDeviceLost() const noexcept override;

private:
    friend class D3D11SwapChainSurface;

    struct Impl;

    // The swap-chain adapter reports DXGI device-removal results through this
    // shared terminal backend state so later resource and queue calls stop.
    void MarkDeviceLost() noexcept;

    D3D11BackendOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

class AERO_API D3D11SwapChainSurface final : public ISurfaceBackend {
public:
    explicit D3D11SwapChainSurface(
        D3D11GraphicsBackend& graphics,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11SwapChainSurface() noexcept override;

    D3D11SwapChainSurface(const D3D11SwapChainSurface&) = delete;
    D3D11SwapChainSurface& operator=(const D3D11SwapChainSurface&) = delete;

    AERO_NODISCARD std::uintptr_t NativeSwapChain() const noexcept;
    AERO_NODISCARD bool OwnsSwapChain() const noexcept;

    AERO_NODISCARD SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept override;
    AERO_NODISCARD Base::Result<void> CreateSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    void DestroySurface() noexcept override;
    AERO_NODISCARD Base::Result<void> ResizeSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept override;
    AERO_NODISCARD Base::Result<ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t frameSerial) noexcept override;
    AERO_NODISCARD Base::Result<void> PresentSurface(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept override;
    void DiscardSurfaceFrame(std::uint64_t frameSerial) noexcept override;
    void NotifySurfaceLost() noexcept override;
    AERO_NODISCARD Base::Result<void> RestoreSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    AERO_NODISCARD bool IsSurfaceLost() const noexcept override;

private:
    struct Impl;

    D3D11GraphicsBackend* graphics_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

struct D3D11SurfaceFrame final {
    SurfaceFrame surface;
    ResourceHandle renderTarget;

    AERO_NODISCARD bool IsValid() const noexcept {
        return surface.frameSerial != 0U && renderTarget.IsValid();
    }
};

class AERO_API D3D11SurfacePresenter final {
public:
    D3D11SurfacePresenter(
        RhiDevice& device,
        D3D11GraphicsBackend& backend,
        SurfaceSession& surface) noexcept;
    ~D3D11SurfacePresenter() noexcept;

    D3D11SurfacePresenter(const D3D11SurfacePresenter&) = delete;
    D3D11SurfacePresenter& operator=(const D3D11SurfacePresenter&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    AERO_NODISCARD Base::Result<D3D11SurfaceFrame> AcquireFrame() noexcept;
    AERO_NODISCARD Base::Result<FenceValue> SubmitAndPresent(
        D3D11SurfaceFrame& frame,
        const GraphicsCommandBuffer& commands) noexcept;
    AERO_NODISCARD Base::Result<void> DiscardFrame(
        D3D11SurfaceFrame& frame) noexcept;
    AERO_NODISCARD Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> CollectGarbage() noexcept;

    AERO_NODISCARD FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    AERO_NODISCARD bool HasFrameInFlight() const noexcept {
        return active_.IsValid();
    }

private:
    RhiDevice* device_ = nullptr;
    D3D11GraphicsBackend* backend_ = nullptr;
    SurfaceSession* surface_ = nullptr;
    D3D11SurfaceFrame active_;
    FenceValue lastSubmittedFence_ = 0U;
    bool initialized_ = false;

    AERO_NODISCARD bool Matches(
        const D3D11SurfaceFrame& frame) const noexcept;
    void ClearFrame(D3D11SurfaceFrame& frame) noexcept;
    AERO_NODISCARD Base::Result<void> RetireRenderTarget(
        ResourceHandle handle,
        FenceValue fence) noexcept;
};

// Submission counters for the most recently completed RenderPlan frame. They
// make the compatibility renderer's pass and draw granularity observable
// without exposing native D3D11 objects to callers.
struct D3D11RenderPlanSubmitStatistics final {
    std::uint32_t renderPassCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t rectangleInstanceCount = 0U;
    std::uint32_t imageInstanceCount = 0U;
    std::uint32_t meshDrawCallCount = 0U;
    std::uint32_t meshInstanceCount = 0U;
    std::uint32_t glyphDrawCallCount = 0U;
    std::uint32_t glyphInstanceCount = 0U;
    std::uint32_t uniformBufferUploadCount = 0U;
    std::uint32_t pipelineBindingCount = 0U;
    std::uint32_t vertexBufferBindingCount = 0U;
    std::uint32_t indexBufferBindingCount = 0U;
    std::uint32_t uniformBufferBindingCount = 0U;
    std::uint32_t textureSamplerBindingCount = 0U;
};

// Consumes immutable Core::RenderPlan snapshots and presents them through the
// D3D11 surface presenter. The device, graphics backend, and presenter must
// outlive this adapter.
class AERO_API D3D11RenderPlanBackend final : public Core::IRenderBackend {
public:
    D3D11RenderPlanBackend(
        RhiDevice& device,
        D3D11GraphicsBackend& graphics,
        D3D11SurfacePresenter& presenter,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderPlanBackend() noexcept override;

    D3D11RenderPlanBackend(const D3D11RenderPlanBackend&) = delete;
    D3D11RenderPlanBackend& operator=(const D3D11RenderPlanBackend&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    // Image resources stay owned by the caller. They must remain alive until
    // no submitted frame can reference them, then be unregistered before destruction.
    AERO_NODISCARD Base::Result<void> RegisterImage(
        Core::RenderImageId image,
        ResourceHandle texture,
        ResourceHandle sampler) noexcept;
    AERO_NODISCARD Base::Result<void> UnregisterImage(
        Core::RenderImageId image) noexcept;

    // Mesh vertex buffers use Float2 position at byte offset zero followed by
    // Float4 vertex color at byte offset eight; meshes use indexed triangles.
    AERO_NODISCARD Base::Result<void> RegisterMesh(
        Core::RenderMeshId mesh,
        ResourceHandle vertexBuffer,
        ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        IndexType indexType = IndexType::UInt16) noexcept;
    AERO_NODISCARD Base::Result<void> UnregisterMesh(
        Core::RenderMeshId mesh) noexcept;

    // Glyph vertices use Float2 position followed by Float2 atlas UV. The
    // sampled atlas is R8Unorm; its alpha coverage is multiplied by tint.
    AERO_NODISCARD Base::Result<void> RegisterGlyphRun(
        Core::RenderGlyphRunId glyphRun,
        ResourceHandle vertexBuffer,
        ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        ResourceHandle atlasTexture,
        ResourceHandle sampler,
        IndexType indexType = IndexType::UInt16) noexcept;
    AERO_NODISCARD Base::Result<void> UnregisterGlyphRun(
        Core::RenderGlyphRunId glyphRun) noexcept;

    AERO_NODISCARD Base::Result<void> Submit(
        const Core::RenderPlan& plan) noexcept override;

    AERO_NODISCARD bool IsInitialized() const noexcept;
    AERO_NODISCARD FenceValue LastSubmittedFence() const noexcept;
    AERO_NODISCARD D3D11RenderPlanSubmitStatistics
    LastSubmitStatistics() const noexcept;

private:
    struct Impl;

    RhiDevice* device_ = nullptr;
    D3D11GraphicsBackend* graphics_ = nullptr;
    D3D11SurfacePresenter* presenter_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

AERO_NODISCARD AERO_API Base::Result<ResourceHandle>
ImportD3D11ExternalRenderTarget(
    RhiDevice& device,
    D3D11GraphicsBackend& backend,
    const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept;

} // namespace Aero::Rhi

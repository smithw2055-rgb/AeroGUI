#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/RenderDevice.hpp"
#include "render/Surface.hpp"

#include <cstdint>

namespace Aero::Graphics {

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

struct D3D11BackendOptions  {
    D3D11DeviceMode deviceMode = D3D11DeviceMode::Hardware;
    D3D11StatePolicy statePolicy = D3D11StatePolicy::HostResetsState;
    bool enableDebugLayer = false;
    bool allowWarpFallback = true;
    std::uintptr_t borrowedDevice = 0U;
    std::uintptr_t borrowedImmediateContext = 0U;
};

struct D3D11ExternalRenderTargetDescriptor  {
    std::uintptr_t texture2D = 0U;
    std::uintptr_t renderTargetView = 0U;
    std::uintptr_t depthStencilView = 0U;
    TextureResourceDescriptor texture;
    std::uint64_t stableId = 0U;
};

class AERO_API D3D11GraphicsBackend  : public GraphicsBackend {
public:
    explicit D3D11GraphicsBackend(
        const D3D11BackendOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11GraphicsBackend() noexcept override;

    D3D11GraphicsBackend(const D3D11GraphicsBackend&) = delete;
    D3D11GraphicsBackend& operator=(const D3D11GraphicsBackend&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    bool IsInitialized() const noexcept;
    std::uintptr_t NativeDevice() const noexcept;
    std::uintptr_t NativeImmediateContext() const noexcept;
    std::uint32_t NativeFeatureLevel() const noexcept;
    std::uint32_t LiveResourceCount() const noexcept;
    FenceValue LastSubmittedFence() const noexcept override;

    Base::Result<void> ImportExternalRenderTarget(
        ResourceHandle handle,
        const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept;

    Base::Result<void> ReadbackTexture(
        ResourceHandle handle,
        Base::Span<std::uint8_t> destination,
        std::uint32_t destinationRowPitch) noexcept;
    Base::Result<std::uint64_t> ReadbackTextureChecksum(
        ResourceHandle handle) noexcept;
    Base::Result<void> WaitForFence(
        FenceValue fence,
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

    DeviceCapabilities Capabilities() const noexcept override;
    GraphicsBackendKind Kind() const noexcept override {
        return GraphicsBackendKind::D3D11;
    }
    GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept override;

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
    Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept override;
    FenceValue CompletedFence() const noexcept override;
    bool IsDeviceLost() const noexcept override;

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

class AERO_API D3D11SwapChainSurface  : public ISurfaceBackend {
public:
    explicit D3D11SwapChainSurface(
        D3D11GraphicsBackend& graphics,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11SwapChainSurface() noexcept override;

    D3D11SwapChainSurface(const D3D11SwapChainSurface&) = delete;
    D3D11SwapChainSurface& operator=(const D3D11SwapChainSurface&) = delete;

    std::uintptr_t NativeSwapChain() const noexcept;
    bool OwnsSwapChain() const noexcept;

    SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept override;
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
    struct Impl;

    D3D11GraphicsBackend* graphics_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

struct D3D11SurfaceFrame  {
    SurfaceFrame surface;
    ResourceHandle renderTarget;

    bool IsValid() const noexcept {
        return surface.frameSerial != 0U && renderTarget.IsValid();
    }
};

class AERO_API D3D11SurfacePresenter  {
public:
    D3D11SurfacePresenter(
        GraphicsDevice& device,
        D3D11GraphicsBackend& backend,
        SurfaceSession& surface) noexcept;
    ~D3D11SurfacePresenter() noexcept;

    D3D11SurfacePresenter(const D3D11SurfacePresenter&) = delete;
    D3D11SurfacePresenter& operator=(const D3D11SurfacePresenter&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<D3D11SurfaceFrame> AcquireFrame() noexcept;
    Base::Result<FenceValue> SubmitAndPresent(
        D3D11SurfaceFrame& frame,
        const CommandList& commands) noexcept;
    Base::Result<void> DiscardFrame(
        D3D11SurfaceFrame& frame) noexcept;
    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;

    FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    bool HasFrameInFlight() const noexcept {
        return active_.IsValid();
    }

private:
    GraphicsDevice* device_ = nullptr;
    D3D11GraphicsBackend* backend_ = nullptr;
    SurfaceSession* surface_ = nullptr;
    D3D11SurfaceFrame active_;
    FenceValue lastSubmittedFence_ = 0U;
    bool initialized_ = false;

    bool Matches(
        const D3D11SurfaceFrame& frame) const noexcept;
    void ClearFrame(D3D11SurfaceFrame& frame) noexcept;
    Base::Result<void> RetireRenderTarget(
        ResourceHandle handle,
        FenceValue fence) noexcept;
};

AERO_API Base::Result<ResourceHandle>
ImportD3D11ExternalRenderTarget(
    GraphicsDevice& device,
    D3D11GraphicsBackend& backend,
    const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept;

} // namespace Aero::Graphics

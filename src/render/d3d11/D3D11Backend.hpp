#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/RenderCommands.hpp"
#include <Aero/RenderDevice.hpp>
#include "render/WindowRenderContext.hpp"

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

struct D3D11CommandQueueOptions  {
    D3D11DeviceMode deviceMode = D3D11DeviceMode::Hardware;
    D3D11StatePolicy statePolicy = D3D11StatePolicy::HostResetsState;
    bool enableDebugLayer = false;
    bool allowWarpFallback = true;
    std::uintptr_t borrowedDevice = 0U;
    std::uintptr_t borrowedImmediateContext = 0U;
};

struct D3D11RenderTargetBinding  {
    std::uintptr_t texture2D = 0U;
    std::uintptr_t renderTargetView = 0U;
    std::uintptr_t depthStencilView = 0U;
    TextureResourceDescriptor texture;
    std::uint64_t stableId = 0U;
};

class AERO_API D3D11CommandQueue {
public:
    explicit D3D11CommandQueue(
        const D3D11CommandQueueOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11CommandQueue() noexcept;

    D3D11CommandQueue(const D3D11CommandQueue&) = delete;
    D3D11CommandQueue& operator=(const D3D11CommandQueue&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    bool IsInitialized() const noexcept;
    std::uintptr_t NativeDevice() const noexcept;
    std::uintptr_t NativeImmediateContext() const noexcept;
    std::uint32_t NativeFeatureLevel() const noexcept;
    std::uint32_t LiveResourceCount() const noexcept;
    FenceValue LastSubmittedFence() const noexcept;

    Base::Result<void> ImportExternalRenderTarget(
        ResourceHandle handle,
        const D3D11RenderTargetBinding& descriptor) noexcept;

    Base::Result<void> ReadbackTexture(
        ResourceHandle handle,
        Base::Span<std::uint8_t> destination,
        std::uint32_t destinationRowPitch) noexcept;
    Base::Result<std::uint64_t> ReadbackTextureChecksum(
        ResourceHandle handle) noexcept;
    Base::Result<void> WaitForFence(
        FenceValue fence,
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

    DeviceCapabilities Capabilities() const noexcept;
    NativeRenderBackendKind Kind() const noexcept {
        return NativeRenderBackendKind::D3D11;
    }
    GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept;

    Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept;
    void DestroyResource(ResourceHandle handle) noexcept;
    Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept;
    Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept;
    Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept;
    FenceValue CompletedFence() const noexcept;
    bool IsDeviceLost() const noexcept;

private:
    friend class D3D11RenderContext;

    struct Impl;

    // The swap-chain adapter reports DXGI device-removal results through this
    // shared terminal backend state so later resource and queue calls stop.
    void MarkDeviceLost() noexcept;

    D3D11CommandQueueOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

class AERO_API D3D11RenderContext {
public:
    explicit D3D11RenderContext(
        D3D11CommandQueue& graphics,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderContext() noexcept;

    D3D11RenderContext(const D3D11RenderContext&) = delete;
    D3D11RenderContext& operator=(const D3D11RenderContext&) = delete;

    std::uintptr_t NativeSwapChain() const noexcept;
    bool OwnsSwapChain() const noexcept;

    WindowRenderContextCaps
    Caps() const noexcept;
    Base::Result<void> Create(
        const WindowRenderContextDescriptor& descriptor) noexcept;
    void Shutdown() noexcept;
    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<RenderTargetBinding>
    AcquireTarget(std::uint64_t frameSerial) noexcept;
    Base::Result<void> Present(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept;
    void DiscardFrame(std::uint64_t frameSerial) noexcept;
    void NotifyLost() noexcept;
    Base::Result<void> Restore(
        const WindowRenderContextDescriptor& descriptor) noexcept;
    bool IsLost() const noexcept;

private:
    struct Impl;

    D3D11CommandQueue* graphics_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};


AERO_API Base::Result<ResourceHandle>
ImportD3D11ExternalRenderTarget(
    Aero::RenderDevice::Impl& device,
    D3D11CommandQueue& backend,
    const D3D11RenderTargetBinding& descriptor) noexcept;

} // namespace Aero::Graphics

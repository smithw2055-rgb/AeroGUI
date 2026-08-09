#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"
#include "render/RenderDeviceState.hpp"
#include <AeroRender/RenderDevice.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Graphics {

struct D3D11RenderDeviceState;

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

struct D3D11RenderDeviceOptions  {
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

class D3D11RenderDevice final : public Aero::Render::RenderDeviceBase {
public:
    explicit D3D11RenderDevice(
        const D3D11RenderDeviceOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderDevice() noexcept override;

    D3D11RenderDevice(const D3D11RenderDevice&) = delete;
    D3D11RenderDevice& operator=(const D3D11RenderDevice&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    ::Aero::RenderBackendKind BackendKind() const noexcept override {
        return ::Aero::RenderBackendKind::D3D11;
    }
    Base::Result<FenceValue> DrawBatch(
        ::Aero::Render::RenderBatch&& batch) noexcept override;
    void NotifyBackendDeviceLost() noexcept override;
    Base::Result<void> RestoreBackendDevice() noexcept override;
    Base::Result<void> WaitBackendIdle(
        std::uint32_t timeoutMilliseconds) noexcept override;
    ::Aero::RenderBackendHealth BackendHealth() const noexcept override;

    bool IsInitialized() const noexcept;
    bool IsReady() const noexcept {
        return IsInitialized() && !deviceLost_ && AreResourcesReady();
    }
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
    ::Aero::Graphics::GraphicsCapabilities
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
        const NativePipelineState& descriptor) noexcept;
    Base::Result<void> Submit(
        const ::Aero::Render::RenderBatch& commands,
        ResourceHandle pipeline,
        FenceValue signalFence) noexcept;
    FenceValue CompletedFence() const noexcept;
    bool IsDeviceLost() const noexcept;

private:
    // The swap-chain adapter reports DXGI device-removal results through this
    // shared terminal backend state so later resource and queue calls stop.
    void MarkDeviceLost() noexcept;

    DeviceCapabilities QueryNativeDeviceCapabilities() const noexcept override {
        return Capabilities();
    }
    NativeRenderBackendKind NativeBackendKind() const noexcept override {
        return Kind();
    }
    ::Aero::Graphics::GraphicsCapabilities
    QueryNativeGraphicsCapabilities() const noexcept override {
        return QueryGraphicsCapabilities();
    }
    Base::Result<void> CreateNativeResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override {
        return CreateResource(handle, descriptor);
    }
    void DestroyNativeResource(ResourceHandle handle) noexcept override {
        D3D11RenderDevice::DestroyResource(handle);
    }
    Base::Result<void> ConfigureNativeTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept override {
        return ConfigureTexture(handle, descriptor);
    }
    Base::Result<void> ConfigureNativeSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept override {
        return ConfigureSampler(handle, descriptor);
    }
    Base::Result<void> ConfigureNativePipeline(
        ResourceHandle handle,
        ::Aero::Render::UiPipelineKey key) noexcept override;
    Base::Result<void> SubmitNativeBatch(
        const ::Aero::Render::RenderBatch& batch,
        ResourceHandle pipeline,
        FenceValue signalFence) noexcept override {
        return Submit(batch, pipeline, signalFence);
    }
    Base::Result<void> UpdateNativeBuffer(
        ResourceHandle buffer,
        std::uint64_t destinationOffset,
        Base::Span<const std::uint8_t> data) noexcept override;
    Base::Result<void> UpdateNativeTexture(
        ResourceHandle texture,
        const TextureRegion& region,
        Base::Span<const std::uint8_t> data) noexcept override;
    FenceValue NativeLastSubmittedFence() const noexcept override {
        return LastSubmittedFence();
    }
    FenceValue NativeCompletedFence() const noexcept override {
        return CompletedFence();
    }
    bool NativeDeviceLost() const noexcept override {
        return IsDeviceLost();
    }

    D3D11RenderDeviceOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    // The backend owns its implementation storage directly. Keeping the
    // platform-heavy state out of this header avoids leaking D3D declarations
    // without paying for a second heap allocation or a source-only Pimpl.
    alignas(std::max_align_t) std::uint8_t stateStorage_[16384]{};
    D3D11RenderDeviceState* state_ = nullptr;
    bool deviceLost_ = false;
};

Base::Result<ResourceHandle>
ImportD3D11ExternalRenderTarget(
    Aero::Render::RenderDeviceBase& device,
    D3D11RenderDevice& backend,
    const D3D11RenderTargetBinding& descriptor) noexcept;

} // namespace Aero::Graphics

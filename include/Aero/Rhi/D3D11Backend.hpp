#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Rhi/Graphics.hpp>

#include <cstdint>

namespace Aero::Rhi {

enum class D3D11DeviceMode : std::uint8_t {
    Hardware = 0U,
    Warp,
    Borrowed
};

struct D3D11BackendOptions final {
    D3D11DeviceMode deviceMode = D3D11DeviceMode::Hardware;
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
    struct Impl;

    D3D11BackendOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

AERO_NODISCARD AERO_API Base::Result<ResourceHandle>
ImportD3D11ExternalRenderTarget(
    RhiDevice& device,
    D3D11GraphicsBackend& backend,
    const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept;

} // namespace Aero::Rhi

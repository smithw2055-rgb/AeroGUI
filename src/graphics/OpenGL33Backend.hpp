#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "Graphics.hpp"
#include "OpenGL33.hpp"
#include "OpenGL33State.hpp"

#include <cstdint>

namespace Aero::Graphics {

struct OpenGL33BackendOptions final {
    GlEmbeddingMode embeddingMode = GlEmbeddingMode::HostReset;
    bool checkErrors = false;
};

struct OpenGL33ExternalRenderTargetDescriptor final {
    GlUInt framebuffer = 0U;
    GlUInt colorTexture = 0U;
    GlUInt depthStencilTexture = 0U;
    TextureResourceDescriptor texture;
    GlContextGeneration contextGeneration = 0U;
    std::uint64_t stableId = 0U;
    bool defaultFramebuffer = false;
};

struct OpenGL33ExternalTextureDescriptor final {
    GlUInt texture = 0U;
    TextureResourceDescriptor descriptor;
    GlContextGeneration contextGeneration = 0U;
    std::uint64_t stableId = 0U;
};

AERO_API Base::Result<void>
ValidateOpenGL33PipelineDescriptor(
    const PipelineDescriptor& descriptor) noexcept;

class AERO_API OpenGL33GraphicsBackend final
    : public GraphicsBackend {
public:
    OpenGL33GraphicsBackend(
        const GlFunctionTable& functions,
        const GlContextContract& context,
        const OpenGL33BackendOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33GraphicsBackend() noexcept override;

    OpenGL33GraphicsBackend(
        const OpenGL33GraphicsBackend&) = delete;
    OpenGL33GraphicsBackend& operator=(
        const OpenGL33GraphicsBackend&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    void NotifyContextLost() noexcept;

    bool IsInitialized() const noexcept;
    std::uint32_t LiveResourceCount() const noexcept;

    Base::Result<void> ImportExternalRenderTarget(
        ResourceHandle handle,
        const OpenGL33ExternalRenderTargetDescriptor& descriptor) noexcept;
    Base::Result<void> ImportExternalTexture(
        ResourceHandle handle,
        const OpenGL33ExternalTextureDescriptor& descriptor) noexcept;

    Base::Result<void> ReadbackTexture(
        ResourceHandle handle,
        Base::Span<std::uint8_t> destination,
        std::uint32_t destinationRowPitch) noexcept;
    Base::Result<std::uint64_t> ReadbackTextureChecksum(
        ResourceHandle handle) noexcept;
    Base::Result<void> WaitForFence(
        FenceValue fence,
        std::uint64_t timeoutNanoseconds =
            UINT64_C(5000000000)) noexcept;

    DeviceCapabilities Capabilities() const noexcept override;
    GraphicsBackendKind Kind() const noexcept override {
        return GraphicsBackendKind::OpenGL33;
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
    FenceValue LastSubmittedFence() const noexcept override;
    FenceValue CompletedFence() const noexcept override;
    bool IsDeviceLost() const noexcept override;

private:
    struct Impl;

    GlFunctionTable functions_;
    GlContextContract context_;
    OpenGL33BackendOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

AERO_API Base::Result<ResourceHandle>
ImportOpenGL33ExternalRenderTarget(
    GraphicsDevice& device,
    OpenGL33GraphicsBackend& backend,
    const OpenGL33ExternalRenderTargetDescriptor& descriptor) noexcept;

AERO_API Base::Result<ResourceHandle>
ImportOpenGL33ExternalTexture(
    GraphicsDevice& device,
    OpenGL33GraphicsBackend& backend,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept;

} // namespace Aero::Graphics

#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"
#include <Aero/Gui/RenderDevice.hpp>
#include "OpenGL33.hpp"
#include "OpenGL33State.hpp"

#include <cstdint>

namespace Aero::Graphics {

struct OpenGL33CommandQueueOptions  {
    GlEmbeddingMode embeddingMode = GlEmbeddingMode::HostReset;
    bool checkErrors = false;
};

struct OpenGL33RenderTargetBinding  {
    GlUInt framebuffer = 0U;
    GlUInt colorTexture = 0U;
    GlUInt depthStencilTexture = 0U;
    TextureResourceDescriptor texture;
    GlContextGeneration contextGeneration = 0U;
    std::uint64_t stableId = 0U;
    bool defaultFramebuffer = false;
};

struct OpenGL33ExternalTextureDescriptor  {
    GlUInt texture = 0U;
    TextureResourceDescriptor descriptor;
    GlContextGeneration contextGeneration = 0U;
    std::uint64_t stableId = 0U;
};

AERO_API Base::Result<void>
ValidateOpenGL33NativePipelineState(
    const NativePipelineState& descriptor) noexcept;

class AERO_API OpenGL33CommandQueue {
public:
    OpenGL33CommandQueue(
        const GlFunctionTable& functions,
        const GlContextBinding& context,
        const OpenGL33CommandQueueOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33CommandQueue() noexcept;

    OpenGL33CommandQueue(
        const OpenGL33CommandQueue&) = delete;
    OpenGL33CommandQueue& operator=(
        const OpenGL33CommandQueue&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    void NotifyContextLost() noexcept;

    bool IsInitialized() const noexcept;
    std::uint32_t LiveResourceCount() const noexcept;

    Base::Result<void> ImportExternalRenderTarget(
        ResourceHandle handle,
        const OpenGL33RenderTargetBinding& descriptor) noexcept;
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

    DeviceCapabilities Capabilities() const noexcept;
    NativeRenderBackendKind Kind() const noexcept {
        return NativeRenderBackendKind::OpenGL33;
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
        const NativePipelineState& descriptor) noexcept;
    Base::Result<void> Submit(
        const ::Aero::Render::Detail::RenderBatch& commands,
        FenceValue signalFence) noexcept;
    FenceValue LastSubmittedFence() const noexcept;
    FenceValue CompletedFence() const noexcept;
    bool IsDeviceLost() const noexcept;

private:
    struct Impl;

    GlFunctionTable functions_;
    GlContextBinding context_;
    OpenGL33CommandQueueOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

AERO_API Base::Result<ResourceHandle>
ImportOpenGL33ExternalRenderTarget(
    Aero::RenderDevice::Impl& device,
    OpenGL33CommandQueue& backend,
    const OpenGL33RenderTargetBinding& descriptor) noexcept;

AERO_API Base::Result<ResourceHandle>
ImportOpenGL33ExternalTexture(
    Aero::RenderDevice::Impl& device,
    OpenGL33CommandQueue& backend,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept;

} // namespace Aero::Graphics

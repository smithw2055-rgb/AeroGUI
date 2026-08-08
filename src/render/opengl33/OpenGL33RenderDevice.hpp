#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"
#include "render/RenderDeviceState.hpp"
#include <Aero/Render/RenderDevice.hpp>
#include <Aero/Render/OpenGL33.hpp>
#include "OpenGL33.hpp"
#include "OpenGL33State.hpp"

#include <cstddef>
#include <cstdint>

namespace Aero::Graphics {

struct OpenGL33RenderDeviceState;

struct OpenGL33RenderDeviceOptions  {
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

Base::Result<void>
ValidateOpenGL33NativePipelineState(
    const NativePipelineState& descriptor) noexcept;

class OpenGL33RenderDevice : public Aero::RenderDevice::Access {
public:
    OpenGL33RenderDevice(
        const GlFunctionTable& functions,
        const GlContextBinding& context,
        const OpenGL33RenderDeviceOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;
    OpenGL33RenderDevice(
        const ::Aero::Render::OpenGL33DeviceOptions& options,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33RenderDevice() noexcept override;

    OpenGL33RenderDevice(
        const OpenGL33RenderDevice&) = delete;
    OpenGL33RenderDevice& operator=(
        const OpenGL33RenderDevice&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    void NotifyContextLost() noexcept;

    ::Aero::Render::RenderBackendKind Backend() const noexcept override {
        return ::Aero::Render::RenderBackendKind::OpenGL33;
    }
    Base::Result<FenceValue> DrawBatch(
        ::Aero::Render::RenderBatch&& batch) noexcept override;
    void NotifyDeviceLost() noexcept override;
    Base::Result<void> RestoreDevice() noexcept override;
    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds) noexcept override;
    ::Aero::Render::BackendHealth GetDeviceHealth() const noexcept override;

    bool IsInitialized() const noexcept;
    bool IsReady() const noexcept {
        return IsInitialized() && !deviceLost_ && AreResourcesReady();
    }
    Base::Result<void> MakeCurrent() noexcept;
    GlContextGeneration ContextGeneration() const noexcept {
        return context_.generation;
    }
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
    FenceValue LastSubmittedFence() const noexcept;
    FenceValue CompletedFence() const noexcept;
    bool IsDeviceLost() const noexcept;

protected:
    explicit OpenGL33RenderDevice(
        Base::IAllocator* allocator) noexcept;
    void ConfigureContext(
        const GlFunctionTable& functions,
        const GlContextBinding& context,
        const OpenGL33RenderDeviceOptions& options) noexcept;

private:
    static GlProcAddress ResolveHost(
        void* context, const char* name) noexcept;
    static bool IsHostCurrent(
        void* context, const void*) noexcept;
    static GlThreadToken CurrentThreadToken(void*) noexcept;

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
        OpenGL33RenderDevice::DestroyResource(handle);
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

    GlFunctionTable functions_;
    GlContextBinding context_;
    OpenGL33RenderDeviceOptions options_;
    ::Aero::Render::OpenGL33DeviceOptions hostOptions_;
    Base::IAllocator* allocator_ = nullptr;
    // Own the backend state in-place while keeping OpenGL implementation
    // types out of this source-only declaration.
    alignas(std::max_align_t) std::uint8_t stateStorage_[16384]{};
    OpenGL33RenderDeviceState* state_ = nullptr;
    bool hostManaged_ = false;
    bool deviceLost_ = false;
};

Base::Result<ResourceHandle>
ImportOpenGL33ExternalRenderTarget(
    Aero::RenderDevice::Access& device,
    OpenGL33RenderDevice& backend,
    const OpenGL33RenderTargetBinding& descriptor) noexcept;

Base::Result<ResourceHandle>
ImportOpenGL33ExternalTexture(
    Aero::RenderDevice::Access& device,
    OpenGL33RenderDevice& backend,
    const OpenGL33ExternalTextureDescriptor& descriptor) noexcept;

} // namespace Aero::Graphics

#pragma once

#include "FrameEncoder.hpp"
#include "render/RenderResources.hpp"
#include "render/Surface.hpp"

#include <thread>

namespace Aero::Render {

// Single backend-neutral renderer owned by each native RenderDevice. It owns
// frame encoding and device-generation-scoped image/mesh/glyph resources.
class Renderer {
public:
    Renderer(
        Graphics::Device& device,
        const FrameShaderSet& shaders,
        std::uint64_t generation,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> RegisterImage(
        RenderImageId image,
        Graphics::ResourceHandle texture,
        Graphics::ResourceHandle sampler) noexcept;
    Base::Result<void> UnregisterImage(RenderImageId image) noexcept;
    Base::Result<void> RegisterMesh(
        RenderMeshId mesh,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::IndexType indexType = Graphics::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(RenderMeshId mesh) noexcept;
    Base::Result<void> RegisterGlyphRun(
        RenderGlyphRunId glyphRun,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::ResourceHandle atlasTexture,
        Graphics::ResourceHandle sampler,
        Graphics::IndexType indexType) noexcept;
    Base::Result<void> UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept;

    // Renderer is the single frame orchestrator: it owns command recording,
    // exactly-one submission, surface completion/presentation, garbage
    // collection, and render-thread affinity. Native backends only acquire and
    // import their target before handing the frame to this boundary.
    Base::Result<Graphics::FenceValue> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    Base::Result<Graphics::FenceValue> RenderOnscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame,
        const RenderTarget& target,
        Graphics::SurfaceSession& surface,
        Graphics::SurfaceFrame& surfaceFrame) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;

    FrameEncoderStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;
    Detail::RenderResources Resources() noexcept;

private:
    struct Impl;

    Base::Result<void> VerifyReady() const noexcept;

    Graphics::Device* device_ = nullptr;
    FrameShaderSet shaders_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    std::uint64_t generation_ = 0U;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render

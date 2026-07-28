#pragma once

#include "../Renderer.hpp"

#include "rhi/OpenGL33Backend.hpp"
#include "rhi/Surface.hpp"

namespace Aero::Render {

using OpenGL33RenderPlanSubmitStatistics = RendererStatistics;

RendererShaderSet MakeOpenGL33RendererShaderSet() noexcept;

class OpenGL33RenderPlanBackend final
    : public Presentation::IRenderBackend {
public:
    OpenGL33RenderPlanBackend(
        Rhi::RhiDevice& device,
        Rhi::OpenGL33GraphicsBackend& graphicsBackend,
        Rhi::SurfaceSession& surface,
        Rhi::GlContextGeneration contextGeneration,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33RenderPlanBackend() noexcept override;

    OpenGL33RenderPlanBackend(
        const OpenGL33RenderPlanBackend&) = delete;
    OpenGL33RenderPlanBackend& operator=(
        const OpenGL33RenderPlanBackend&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    Base::Result<void> RegisterImage(
        Presentation::RenderImageId image,
        Rhi::ResourceHandle texture,
        Rhi::ResourceHandle sampler) noexcept;
    Base::Result<void> UnregisterImage(
        Presentation::RenderImageId image) noexcept;
    Base::Result<void> RegisterMesh(
        Presentation::RenderMeshId mesh,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::IndexType indexType =
            Rhi::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(
        Presentation::RenderMeshId mesh) noexcept;
    Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept override;
    bool IsInitialized() const noexcept;
    Rhi::FenceValue LastSubmittedFence() const noexcept;
    OpenGL33RenderPlanSubmitStatistics
    LastSubmitStatistics() const noexcept;

private:
    struct Impl;
    Base::Result<void> RegisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType) noexcept;
    Base::Result<void> UnregisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept;
    void* QueryInternalService(
        std::uint64_t service) noexcept override;

    Rhi::RhiDevice* device_ = nullptr;
    Rhi::OpenGL33GraphicsBackend* graphicsBackend_ = nullptr;
    Rhi::SurfaceSession* surface_ = nullptr;
    Rhi::GlContextGeneration contextGeneration_ = 0U;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    std::uint64_t textGeneration_ = 0U;
};

} // namespace Aero::Render

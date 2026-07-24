#pragma once

#include <Aero/Render/Renderer.hpp>
#include <Aero/Rhi/D3D11Backend.hpp>

namespace Aero::Render {

using D3D11RenderPlanSubmitStatistics = RendererStatistics;

// Thin compatibility adapter from Presentation::IRenderBackend to the shared
// backend-neutral Renderer. D3D11 owns only surface import/present and command
// execution; all RenderPlan traversal, state resolution, and batching live in
// AeroRender.
class AERO_API D3D11RenderPlanBackend final
    : public Presentation::IRenderBackend {
public:
    D3D11RenderPlanBackend(
        Rhi::RhiDevice& device,
        Rhi::D3D11SurfacePresenter& presenter,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderPlanBackend() noexcept override;

    D3D11RenderPlanBackend(const D3D11RenderPlanBackend&) = delete;
    D3D11RenderPlanBackend& operator=(const D3D11RenderPlanBackend&) = delete;

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
        Rhi::IndexType indexType = Rhi::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(
        Presentation::RenderMeshId mesh) noexcept;

    Base::Result<void> RegisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType = Rhi::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept;

    Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept override;

    bool IsInitialized() const noexcept;
    Rhi::FenceValue LastSubmittedFence() const noexcept;
    D3D11RenderPlanSubmitStatistics
    LastSubmitStatistics() const noexcept;

private:
    struct Impl;

    Rhi::RhiDevice* device_ = nullptr;
    Rhi::D3D11SurfacePresenter* presenter_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render

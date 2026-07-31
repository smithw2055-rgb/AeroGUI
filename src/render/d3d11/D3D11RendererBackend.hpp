#pragma once

#include "../DisplayList.hpp"

#include "../Renderer.hpp"

#include "rhi/D3D11Backend.hpp"

namespace Aero::Render {

using D3D11RenderPlanSubmitStatistics = RendererStatistics;

RendererShaderSet MakeD3D11RendererShaderSet() noexcept;

class D3D11RenderPlanBackend final
    : public Render::IRenderBackend {
public:
    D3D11RenderPlanBackend(
        Rhi::RhiDevice& device,
        Rhi::D3D11SurfacePresenter& presenter,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderPlanBackend() noexcept override;

    D3D11RenderPlanBackend(
        const D3D11RenderPlanBackend&) = delete;
    D3D11RenderPlanBackend& operator=(
        const D3D11RenderPlanBackend&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    Base::Result<void> RegisterImage(
        Render::RenderImageId image,
        Rhi::ResourceHandle texture,
        Rhi::ResourceHandle sampler) noexcept;
    Base::Result<void> UnregisterImage(
        Render::RenderImageId image) noexcept;
    Base::Result<void> RegisterMesh(
        Render::RenderMeshId mesh,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::IndexType indexType =
            Rhi::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(
        Render::RenderMeshId mesh) noexcept;
    Base::Result<void> Submit(
        const Render::RenderPlan& plan) noexcept override;
    bool IsInitialized() const noexcept;
    Rhi::FenceValue LastSubmittedFence() const noexcept;
    D3D11RenderPlanSubmitStatistics
    LastSubmitStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;

private:
    struct Impl;
    Base::Result<void> RegisterGlyphRun(
        Render::RenderGlyphRunId glyphRun,
        Rhi::ResourceHandle vertexBuffer,
        Rhi::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Rhi::ResourceHandle atlasTexture,
        Rhi::ResourceHandle sampler,
        Rhi::IndexType indexType) noexcept;
    Base::Result<void> UnregisterGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept;
    void* QueryInternalService(
        std::uint64_t service) noexcept override;

    Rhi::RhiDevice* device_ = nullptr;
    Rhi::D3D11SurfacePresenter* presenter_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    std::uint64_t textGeneration_ = 0U;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render

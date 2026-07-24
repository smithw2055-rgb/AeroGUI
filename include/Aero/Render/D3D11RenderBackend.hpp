#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Render/Renderer.hpp>
#include <Aero/Rhi/D3D11Backend.hpp>

namespace Aero::Render {

// Thin platform adapter: it acquires/presents the D3D11 surface and supplies
// offline DXBC packages. All RenderPlan traversal and batching lives in Renderer.
class AERO_API D3D11RenderBackend final {
public:
    D3D11RenderBackend(
        Rhi::RhiDevice& device,
        Rhi::D3D11GraphicsBackend& graphics,
        Rhi::SurfaceSession& surface,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11RenderBackend() noexcept;

    D3D11RenderBackend(const D3D11RenderBackend&) = delete;
    D3D11RenderBackend& operator=(const D3D11RenderBackend&) = delete;

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
        const Presentation::RenderPlan& plan) noexcept;

    bool IsInitialized() const noexcept;
    Rhi::FenceValue LastSubmittedFence() const noexcept;
    const RendererStatistics& LastStatistics() const noexcept;

private:
    Rhi::RhiDevice* device_ = nullptr;
    Rhi::D3D11GraphicsBackend* graphics_ = nullptr;
    Rhi::SurfaceSession* surface_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Renderer* renderer_ = nullptr;
    Rhi::FenceValue lastSubmittedFence_ = 0U;
};

} // namespace Aero::Render

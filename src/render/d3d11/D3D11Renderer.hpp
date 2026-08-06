#pragma once

#include "../DisplayList.hpp"

#include "../DeviceRenderer.hpp"

#include "render/d3d11/D3D11Backend.hpp"

namespace Aero::Render::Detail { struct TextResources; struct MeshResources; struct ImageResources; }

namespace Aero::Render {

using D3D11RendererStatistics = FrameEncoderStatistics;

FrameShaderSet MakeD3D11RendererShaderSet() noexcept;

// D3D11-only surface acquisition and presentation adapter. DeviceRenderer owns
// the backend-neutral encoder and all shared GPU resource registries.
class D3D11Renderer {
public:
    D3D11Renderer(
        Graphics::Device& device,
        Graphics::D3D11SurfacePresenter& presenter,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~D3D11Renderer() noexcept;

    D3D11Renderer(
        const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(
        const D3D11Renderer&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    Base::Result<void> RegisterImage(
        Render::RenderImageId image,
        Graphics::ResourceHandle texture,
        Graphics::ResourceHandle sampler) noexcept;
    Base::Result<void> UnregisterImage(
        Render::RenderImageId image) noexcept;
    Base::Result<void> RegisterMesh(
        Render::RenderMeshId mesh,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::IndexType indexType =
            Graphics::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(
        Render::RenderMeshId mesh) noexcept;
    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const Integration::RenderFrame& plan) noexcept;
    Base::Result<void> Render(
        const void* rendererToken,
        const Integration::RenderFrame& plan,
        Graphics::LoadOperation load) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    bool IsInitialized() const noexcept;
    Graphics::FenceValue LastSubmittedFence() const noexcept;
    D3D11RendererStatistics
    LastSubmitStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    Aero::Render::Detail::TextResources* GetTextResources() noexcept;
    Aero::Render::Detail::MeshResources* GetMeshResources() noexcept;
    Aero::Render::Detail::ImageResources* GetImageResources() noexcept;

private:
    struct Impl;
    Base::Result<void> RegisterGlyphRun(
        Render::RenderGlyphRunId glyphRun,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::ResourceHandle atlasTexture,
        Graphics::ResourceHandle sampler,
        Graphics::IndexType indexType) noexcept;
    Base::Result<void> UnregisterGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept;

    Graphics::Device* device_ = nullptr;
    Graphics::D3D11SurfacePresenter* presenter_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render

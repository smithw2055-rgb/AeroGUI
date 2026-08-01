#pragma once

#include "../DisplayList.hpp"

#include "../Renderer.hpp"

#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/Surface.hpp"

namespace Aero::Detail { struct TextBackendServices; struct MeshBackendServices; struct ImageBackendServices; }

namespace Aero::Render {

using OpenGL33RendererStatistics = RendererStatistics;

RendererShaderSet MakeOpenGL33RendererShaderSet() noexcept;

class OpenGL33Renderer final {
public:
    OpenGL33Renderer(
        Graphics::GraphicsDevice& device,
        Graphics::OpenGL33GraphicsBackend& graphicsBackend,
        Graphics::SurfaceSession& surface,
        Graphics::GlContextGeneration contextGeneration,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~OpenGL33Renderer() noexcept;

    OpenGL33Renderer(
        const OpenGL33Renderer&) = delete;
    OpenGL33Renderer& operator=(
        const OpenGL33Renderer&) = delete;

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
    Base::Result<void> Submit(
        const Render::RenderFrame& plan) noexcept;
    bool IsInitialized() const noexcept;
    Graphics::FenceValue LastSubmittedFence() const noexcept;
    OpenGL33RendererStatistics
    LastSubmitStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    Aero::Detail::TextBackendServices* TextServices() noexcept;
    Aero::Detail::MeshBackendServices* MeshServices() noexcept;
    Aero::Detail::ImageBackendServices* ImageServices() noexcept;

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

    Graphics::GraphicsDevice* device_ = nullptr;
    Graphics::OpenGL33GraphicsBackend* graphicsBackend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::GlContextGeneration contextGeneration_ = 0U;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    std::uint64_t textGeneration_ = 0U;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render

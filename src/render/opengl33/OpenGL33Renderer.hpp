#pragma once

#include "../DisplayList.hpp"

#include "../DeviceRenderer.hpp"

#include "render/opengl33/OpenGL33Backend.hpp"
#include "render/Surface.hpp"

namespace Aero::Render::Detail { struct TextResources; struct MeshResources; struct ImageResources; }

namespace Aero::Render {

using OpenGL33RendererStatistics = FrameEncoderStatistics;

FrameShaderSet MakeOpenGL33RendererShaderSet() noexcept;

// OpenGL-only context, target acquisition and presentation adapter. Shared
// encoding and GPU resource ownership live in DeviceRenderer.
class OpenGL33Renderer {
public:
    OpenGL33Renderer(
        Graphics::Device& device,
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
    OpenGL33RendererStatistics
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
    Graphics::OpenGL33GraphicsBackend* graphicsBackend_ = nullptr;
    Graphics::SurfaceSession* surface_ = nullptr;
    Graphics::GlContextGeneration contextGeneration_ = 0U;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render

#pragma once

#include "DisplayList.hpp"

#include "render/RenderTree.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include "render/RenderDevice.hpp"

namespace Aero::Render {

class D3D11Renderer;
class OpenGL33Renderer;
namespace Detail {
class RendererGlyphRunSink;
}

struct RendererShaderSet  {
    Graphics::ShaderDescriptor rectangleVertex;
    Graphics::ShaderDescriptor rectangleFragment;
    Graphics::ShaderDescriptor imageVertex;
    Graphics::ShaderDescriptor imageFragment;
    Graphics::ShaderDescriptor meshVertex;
    Graphics::ShaderDescriptor meshFragment;
    Graphics::ShaderDescriptor glyphVertex;
    Graphics::ShaderDescriptor glyphFragment;
    Graphics::GraphicsTextureFormat colorFormat =
        Graphics::GraphicsTextureFormat::Bgra8Unorm;
};

struct RenderTarget  {
    Graphics::ResourceHandle color;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

struct RendererStatistics  {
    std::uint32_t renderPassCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t rectangleInstanceCount = 0U;
    std::uint32_t imageInstanceCount = 0U;
    std::uint32_t meshDrawCallCount = 0U;
    std::uint32_t meshInstanceCount = 0U;
    std::uint32_t glyphDrawCallCount = 0U;
    std::uint32_t glyphInstanceCount = 0U;
    std::uint32_t uniformBufferUploadCount = 0U;
    std::uint32_t pipelineBindingCount = 0U;
    std::uint32_t vertexBufferBindingCount = 0U;
    std::uint32_t indexBufferBindingCount = 0U;
    std::uint32_t uniformBufferBindingCount = 0U;
    std::uint32_t textureSamplerBindingCount = 0U;
};

class Renderer  {
public:
    Renderer(
        Graphics::GraphicsDevice& device,
        const RendererShaderSet& shaders,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

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
    Base::Result<Graphics::CommandList> Record(
        const Integration::RenderFrame& plan,
        const RenderTarget& target) noexcept;
    RendererStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;

private:
    friend class D3D11Renderer;
    friend class OpenGL33Renderer;
    friend class Detail::RendererGlyphRunSink;

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
    RendererShaderSet shaders_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render

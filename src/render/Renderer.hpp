#pragma once

#include "presentation/RenderingInternal.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include "rhi/Graphics.hpp"

namespace Aero::Render {

class D3D11RenderPlanBackend;
class OpenGL33RenderPlanBackend;
namespace Detail {
class RendererGlyphRunSink;
}

struct RendererShaderSet final {
    Rhi::ShaderDescriptor rectangleVertex;
    Rhi::ShaderDescriptor rectangleFragment;
    Rhi::ShaderDescriptor imageVertex;
    Rhi::ShaderDescriptor imageFragment;
    Rhi::ShaderDescriptor meshVertex;
    Rhi::ShaderDescriptor meshFragment;
    Rhi::ShaderDescriptor glyphVertex;
    Rhi::ShaderDescriptor glyphFragment;
    Rhi::GraphicsTextureFormat colorFormat =
        Rhi::GraphicsTextureFormat::Bgra8Unorm;
};

struct RenderTarget final {
    Rhi::ResourceHandle color;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

struct RendererStatistics final {
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

class Renderer final {
public:
    Renderer(
        Rhi::RhiDevice& device,
        const RendererShaderSet& shaders,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

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
    Base::Result<Rhi::CommandList> Record(
        const Presentation::RenderPlan& plan,
        const RenderTarget& target) noexcept;
    RendererStatistics LastStatistics() const noexcept;

private:
    friend class D3D11RenderPlanBackend;
    friend class OpenGL33RenderPlanBackend;
    friend class Detail::RendererGlyphRunSink;

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

    Rhi::RhiDevice* device_ = nullptr;
    RendererShaderSet shaders_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render

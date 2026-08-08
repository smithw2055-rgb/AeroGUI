#pragma once

#include "DisplayList.hpp"
#include "render/RenderTree.hpp"
#include "render/GraphicsDevice.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Render {
class Renderer;
}

namespace Aero::Render::Detail {

class CommandEncoderGlyphRunSink;

struct CommandEncoderShaderSet {
    Graphics::ShaderDescriptor rectangleVertex;
    Graphics::ShaderDescriptor rectangleFragment;
    Graphics::ShaderDescriptor imageVertex;
    Graphics::ShaderDescriptor imageFragment;
    Graphics::ShaderDescriptor maskVertex;
    Graphics::ShaderDescriptor maskFragment;
    Graphics::ShaderDescriptor effectVertex;
    Graphics::ShaderDescriptor effectFragment;
    Graphics::ShaderDescriptor meshVertex;
    Graphics::ShaderDescriptor meshFragment;
    Graphics::ShaderDescriptor glyphVertex;
    Graphics::ShaderDescriptor glyphFragment;
    Graphics::GraphicsTextureFormat colorFormat =
        Graphics::GraphicsTextureFormat::Bgra8Unorm;
};

struct FrameTarget {
    Graphics::ResourceHandle color;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    Graphics::LoadOperation load = Graphics::LoadOperation::Load;
};

struct CommandEncoderStatistics {
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

// Low-level command encoder. It is an implementation detail owned by the one
// semantic Render::Renderer and does not form a peer renderer lifecycle.
class CommandEncoder {
public:
    CommandEncoder(
        Graphics::Device& device,
        const CommandEncoderShaderSet& shaders,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~CommandEncoder() noexcept;

    CommandEncoder(const CommandEncoder&) = delete;
    CommandEncoder& operator=(const CommandEncoder&) = delete;

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
        Graphics::IndexType indexType = Graphics::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(Render::RenderMeshId mesh) noexcept;
    Base::Result<Graphics::CommandList> Record(
        const ::Aero::Render::Detail::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    Base::Result<Graphics::CommandList> RecordOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& plan) noexcept;
    Base::Result<Graphics::CommandList> RecordOnscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    CommandEncoderStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;

private:
    friend class ::Aero::Render::Renderer;
    friend class CommandEncoderGlyphRunSink;

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
    CommandEncoderShaderSet shaders_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render::Detail

namespace Aero::Render {

using FrameShaderSet = Detail::CommandEncoderShaderSet;
using FrameEncoderStatistics = Detail::CommandEncoderStatistics;
using FrameTarget = Detail::FrameTarget;

} // namespace Aero::Render

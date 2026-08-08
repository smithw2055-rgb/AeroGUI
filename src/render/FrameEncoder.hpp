#pragma once

#include "DisplayList.hpp"
#include "render/RenderTree.hpp"
#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"
#include <Aero/Gui/RenderDevice.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Render {
class Renderer;
}

namespace Aero::Render::Detail {

class BatchGlyphRunSink;

struct BatchShaderSet {
    Graphics::NativeShaderProgram rectangleVertex;
    Graphics::NativeShaderProgram rectangleFragment;
    Graphics::NativeShaderProgram imageVertex;
    Graphics::NativeShaderProgram imageFragment;
    Graphics::NativeShaderProgram maskVertex;
    Graphics::NativeShaderProgram maskFragment;
    Graphics::NativeShaderProgram effectVertex;
    Graphics::NativeShaderProgram effectFragment;
    Graphics::NativeShaderProgram meshVertex;
    Graphics::NativeShaderProgram meshFragment;
    Graphics::NativeShaderProgram glyphVertex;
    Graphics::NativeShaderProgram glyphFragment;
    Graphics::GraphicsTextureFormat colorFormat =
        Graphics::GraphicsTextureFormat::Bgra8Unorm;
};

struct FrameTarget {
    Graphics::ResourceHandle color;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    Graphics::LoadOperation load = Graphics::LoadOperation::Load;
};

struct BatchStatistics {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
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
    bool batchingEnabled = true;
};

// Renderer-owned composer that turns retained UI frames directly into native
// device batches; it has no independent renderer or generic command lifetime.
class BatchComposer {
public:
    BatchComposer(
        Aero::RenderDevice::Impl& device,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~BatchComposer() noexcept;

    BatchComposer(const BatchComposer&) = delete;
    BatchComposer& operator=(const BatchComposer&) = delete;

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
    Base::Result<::Aero::Render::Detail::RenderBatch> Record(
        const ::Aero::Render::Detail::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    Base::Result<::Aero::Render::Detail::RenderBatch> RecordOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& plan) noexcept;
    Base::Result<::Aero::Render::Detail::RenderBatch> RecordOnscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    BatchStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;

private:
    friend class ::Aero::Render::Renderer;
    friend class BatchGlyphRunSink;

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

    Aero::RenderDevice::Impl* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Render::Detail

namespace Aero::Render {

using BackendShaderCatalog = Detail::BatchShaderSet;
using FrameEncoderStatistics = Detail::BatchStatistics;
using FrameTarget = Detail::FrameTarget;

} // namespace Aero::Render

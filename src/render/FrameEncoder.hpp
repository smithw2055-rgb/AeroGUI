#pragma once

#include "DisplayList.hpp"
#include "render/RenderTree.hpp"
#include "render/GraphicsTypes.hpp"
#include "render/RenderBatch.hpp"
#include <AeroRender/RenderDevice.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Render {

class FrameGlyphRunSink;

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

struct ClipState {
    Aero::Rect rect;
    Media::Transform2D transform;
    Aero::Rect bounds;
};

struct NodeState {
    RenderNodeId id = InvalidRenderNodeId;
    Media::Transform2D transform;
    ClipState clip;
    bool clipsToBounds = false;
    double opacity = 1.0;
    std::uint32_t parentIndex = UINT32_MAX;
    RenderNodeId containingEffect = InvalidRenderNodeId;
    std::uint32_t containingEffectCount = 0U;
};

struct ImageBinding {
    RenderImageId id = InvalidRenderImageId;
    Graphics::ResourceHandle texture;
    Graphics::ResourceHandle sampler;
};

struct GradientRampBinding {
    std::uintptr_t key = 0U;
    std::uint64_t revision = 0U;
    Graphics::ResourceHandle texture;
};

struct MeshBinding {
    RenderMeshId id = InvalidRenderMeshId;
    Graphics::ResourceHandle vertexBuffer;
    Graphics::ResourceHandle indexBuffer;
    std::uint32_t indexCount = 0U;
    Graphics::IndexType indexType = Graphics::IndexType::UInt16;
};

struct GlyphBinding {
    RenderGlyphRunId id = InvalidRenderGlyphRunId;
    Graphics::ResourceHandle vertexBuffer;
    Graphics::ResourceHandle indexBuffer;
    std::uint32_t indexCount = 0U;
    Graphics::ResourceHandle atlasTexture;
    Graphics::ResourceHandle sampler;
    Graphics::IndexType indexType = Graphics::IndexType::UInt16;
};

struct EffectSurface {
    RenderNodeId owner = InvalidRenderNodeId;
    Graphics::ResourceHandle content;
    Graphics::ResourceHandle scratch;
    Graphics::ResourceHandle result;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    Aero::Rect logicalBounds;
    bool empty = false;
};

struct ViewSurface {
    Graphics::ResourceHandle target;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t version = 0U;
    BatchStatistics statistics;
    bool prepared = false;
};

// Renderer-owned composer that turns retained UI frames directly into native
// device batches; it has no independent renderer or generic command lifetime.
class UiFrameEncoder {
public:
    UiFrameEncoder(
        Aero::Render::RenderDeviceBase& device,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~UiFrameEncoder() noexcept;

    UiFrameEncoder(const UiFrameEncoder&) = delete;
    UiFrameEncoder& operator=(const UiFrameEncoder&) = delete;

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
    Base::Result<Graphics::FenceValue> Record(
        const ::Aero::Render::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    Base::Result<Graphics::FenceValue> RecordOffscreen(
        const ::Aero::Render::RenderFrame& plan) noexcept;
    Base::Result<Graphics::FenceValue> RecordOnscreen(
        const ::Aero::Render::RenderFrame& plan,
        const FrameTarget& target) noexcept;
    BatchStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;

private:
    friend class FrameGlyphRunSink;

    struct State {
        explicit State(Base::IAllocator* allocator) noexcept
            : nodes(allocator),
              transforms(allocator),
              clips(allocator),
              opacities(allocator),
              nodePath(allocator),
              images(allocator),
              gradientRamps(allocator),
              meshes(allocator),
              glyphRuns(allocator),
              effectSurfaces(allocator),
              viewSurfaces(allocator) {}

        Graphics::ResourceHandle vertexBuffer;
        Graphics::ResourceHandle uniformBuffer;
        Graphics::ResourceHandle imageUniformBuffer;
        Graphics::ResourceHandle maskUniformBuffer;
        Graphics::ResourceHandle effectUniformBuffer;
        Graphics::ResourceHandle meshUniformBuffer;
        Graphics::ResourceHandle glyphUniformBuffer;
        Base::Vector<NodeState> nodes;
        Base::Vector<Media::Transform2D> transforms;
        Base::Vector<ClipState> clips;
        Base::Vector<double> opacities;
        Base::Vector<std::uint32_t> nodePath;
        Base::Vector<ImageBinding> images;
        Base::Vector<GradientRampBinding> gradientRamps;
        Base::Vector<MeshBinding> meshes;
        Base::Vector<GlyphBinding> glyphRuns;
        Base::Vector<EffectSurface> effectSurfaces;
        Base::Vector<ViewSurface> viewSurfaces;
        Graphics::ResourceHandle effectSampler;
        BatchStatistics lastStatistics;
        bool batchingEnabled = true;
        bool initialized = false;
    };
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

    Aero::Render::RenderDeviceBase* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    State state_;
};

} // namespace Aero::Render

namespace Aero::Render {

using BackendShaderCatalog = BatchShaderSet;
using FrameEncoderStatistics = BatchStatistics;
using FrameTarget = FrameTarget;

} // namespace Aero::Render

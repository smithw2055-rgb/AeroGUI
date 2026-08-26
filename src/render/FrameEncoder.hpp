#pragma once

#include "DisplayList.hpp"
#include "RenderTree.hpp"
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/Texture.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Render {

struct FrameStatistics {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t vertexCount = 0U;
    std::uint32_t indexCount = 0U;
    std::uint32_t batchCount = 0U;
};

// A single glyph drawn as a textured quad. Position and UVs are in pixels of
// the source atlas page; the encoder applies the active transform.
struct RenderGlyphQuad {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;
    std::uint32_t page = 0U;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Unified UiFrameEncoder that turns retained UI frames directly into RenderDevice Batch draw calls
////////////////////////////////////////////////////////////////////////////////////////////////////
class UiFrameEncoder {
public:
    explicit UiFrameEncoder(
        RenderDevice& device,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~UiFrameEncoder() noexcept;

    UiFrameEncoder(const UiFrameEncoder&) = delete;
    UiFrameEncoder& operator=(const UiFrameEncoder&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept { return initialized_; }

    Base::Result<void> RegisterImage(
        RenderImageId imageId,
        Ref<Texture> texture) noexcept;
    void UnregisterImage(RenderImageId imageId) noexcept;

    Base::Result<void> RegisterGlyphAtlas(
        std::uint32_t page,
        Ref<Texture> texture) noexcept;

    Base::Result<void> RegisterGlyphRun(
        RenderGlyphRunId glyphRun,
        Base::Span<const RenderGlyphQuad> quads) noexcept;
    void UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept;

    Base::Result<void> RegisterMesh(
        RenderMeshId mesh,
        Base::Span<const Point> vertices,
        Base::Span<const std::uint32_t> indices) noexcept;
    void UnregisterMesh(RenderMeshId mesh) noexcept;

    Base::Result<void> RecordOffscreen(
        const RenderFrame& frame) noexcept;

    Base::Result<void> RecordOnscreen(
        const RenderFrame& frame,
        RenderTarget& target) noexcept;

    FrameStatistics LastStatistics() const noexcept { return stats_; }

private:
    struct ImageEntry {
        RenderImageId id = InvalidRenderImageId;
        Ref<Texture> texture;
    };

    struct AtlasEntry {
        std::uint32_t page = 0U;
        Ref<Texture> texture;
    };

    struct GradientEntry {
        std::uintptr_t brushIdentity = 0U;
        std::uint64_t revision = 0U;
        Ref<Texture> texture;
    };

    struct GlyphRunEntry {
        RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
        Base::Vector<RenderGlyphQuad> quads;
    };

    struct MeshEntry {
        RenderMeshId mesh = InvalidRenderMeshId;
        Base::Vector<Point> vertices;
        Base::Vector<std::uint32_t> indices;
    };

    struct OffscreenTargetEntry {
        RenderNodeId nodeId = InvalidRenderNodeId;
        bool isMask = false;
        Ref<RenderTarget> target;
        std::uint32_t width = 0U;
        std::uint32_t height = 0U;
    };

    struct ClipEntry {
        Rect rect;
        ProjectiveTransform2D transform;
        bool geometry = false;
        std::uint32_t vertexOffset = 0U;
        std::uint32_t vertexCount = 0U;
        std::uint32_t indexOffset = 0U;
        std::uint32_t indexCount = 0U;
    };

    void ResetFrame() noexcept;
    void FlushBatch() noexcept;
    Texture* FindImage(RenderImageId id) const noexcept;
    Texture* FindAtlas(std::uint32_t page) const noexcept;
    const Base::Vector<RenderGlyphQuad>* FindGlyphRun(
        RenderGlyphRunId glyphRun) const noexcept;
    const MeshEntry* FindMesh(RenderMeshId mesh) const noexcept;
    Texture* GetOrCreateGradientRamp(const RenderGradientRampSnapshot& ramp) noexcept;
    RenderTarget* GetOrCreateOffscreenTarget(
        RenderNodeId nodeId, std::uint32_t width, std::uint32_t height,
        bool isMask = false) noexcept;

    void EmitQuad(
        const Point points[4],
        const Point uvs[4],
        Color color) noexcept;

    void EmitQuadWithColors(
        const Point points[4],
        const Point uvs[4],
        const Color colors[4],
        double opacity) noexcept;

    void EmitTriangleFan(
        const Point* perimeter,
        std::uint32_t perimeterCount,
        Point centroid,
        Color color) noexcept;

    void EmitClipQuad(
        const Rect& rect,
        const ProjectiveTransform2D& transform,
        std::uint8_t stencilMode,
        std::uint8_t stencilRef) noexcept;

    void EmitClipTriangles(
        Base::Span<const Point> vertices,
        Base::Span<const std::uint32_t> indices,
        std::uint32_t vertexOffset,
        std::uint32_t vertexCount,
        std::uint32_t indexOffset,
        std::uint32_t indexCount,
        const ProjectiveTransform2D& transform,
        std::uint8_t stencilMode,
        std::uint8_t stencilRef) noexcept;

    void ClearRenderTarget(
        std::uint32_t width,
        std::uint32_t height) noexcept;

    void SetContentStencil() noexcept;

    void EnsureBatchBlend(Shader::Enum shader) noexcept;

    void SetBatchImage(
        Shader::Enum shader,
        Texture* texture,
        Texture* maskTexture = nullptr) noexcept;

    void CompositeOffscreen(
        const RenderNodeSnapshot& node,
        RenderTarget* offscreen,
        RenderTarget* maskTarget,
        const ProjectiveTransform2D& nodeTransform,
        double nodeOpacity,
        double dpi,
        const RenderFrame& frame) noexcept;

    void EmitMaskBrush(
        const RenderMaskSnapshot& mask,
        double width,
        double height,
        const RenderFrame& frame) noexcept;

    void ProcessCommand(
        const RenderCommand& cmd,
        const ProjectiveTransform2D& currentTransform,
        double currentOpacity) noexcept;

    RenderBlendMode::Enum currentBlendMode_ = BlendMode::SrcOver;

    RenderDevice* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;

    Base::Vector<ImageEntry> images_{allocator_};
    Base::Vector<AtlasEntry> atlases_{allocator_};
    Base::Vector<GradientEntry> gradients_{allocator_};
    Base::Vector<GlyphRunEntry> glyphRuns_{allocator_};
    Base::Vector<MeshEntry> meshes_{allocator_};
    Base::Vector<OffscreenTargetEntry> offscreenTargets_{allocator_};
    Base::Vector<ClipEntry> clipStack_{allocator_};
    float offscreenSizeUniform_[2] = {0.0F, 0.0F};
    float customEffectUniforms_[4] = {0.0F, 0.0F, 0.0F, 0.0F};

    std::uint8_t clipDepth_ = 0U;

    uint8_t* mappedVertices_ = nullptr;
    uint16_t* mappedIndices_ = nullptr;
    uint32_t currentVertexOffset_ = 0U;
    uint32_t currentVertexCount_ = 0U;
    uint32_t currentIndexCount_ = 0U;

    Batch currentBatch_{};
    FrameStatistics stats_{};
    bool initialized_ = false;
    bool inRender_ = false;
};

using FrameEncoderStatistics = FrameStatistics;

} // namespace Aero::Render

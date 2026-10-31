#pragma once

#include "DisplayList.hpp"
#include "RenderTree.hpp"
#include "common/StateCache.hpp"
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <AeroRender/Texture.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/HashMap.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

namespace Aero::Render {

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

class RenderResourceTable {
public:
    struct ImageEntry {
        RenderImageId id = InvalidRenderImageId;
        Base::Ref<Texture> texture;
    };
    struct AtlasEntry {
        std::uint32_t page = 0U;
        Base::Ref<Texture> texture;
    };
    struct GradientEntry {
        std::uintptr_t brushIdentity = 0U;
        std::uint64_t revision = 0U;
        Base::Ref<Texture> texture;
    };
    struct GlyphRunEntry {
        RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
        Base::Vector<RenderGlyphQuad> quads;
    };
    struct MeshEntry {
        RenderMeshId mesh = InvalidRenderMeshId;
        Base::Vector<Point> vertices;
        Base::Vector<float> coverages;
        Base::Vector<std::uint32_t> indices;
    };
    struct OffscreenTargetEntry {
        RenderNodeId nodeId = InvalidRenderNodeId;
        bool isMask = false;
        bool inUse = false;
        Base::Ref<RenderTarget> target;
        std::uint32_t width = 0U;
        std::uint32_t height = 0U;
    };

    explicit RenderResourceTable(Base::IAllocator* alloc = nullptr) noexcept
        : imageMap_(alloc), atlasMap_(alloc), gradientRamps_(alloc),
          glyphRuns_(alloc), meshes_(alloc), offscreenTargets_(alloc) {}

    void Clear() noexcept {
        lastImageId_ = InvalidRenderImageId;
        lastImage_ = nullptr;
        lastGlyphRunId_ = InvalidRenderGlyphRunId;
        lastGlyphRun_ = nullptr;
        lastMeshId_ = InvalidRenderMeshId;
        lastMesh_ = nullptr;
        imageMap_.Clear();
        atlasMap_.Clear();
        glyphRunIndex_.Clear();
        meshIndex_.Clear();
        gradientRamps_.Clear();
        glyphRuns_.Clear();
        meshes_.Clear();
        offscreenTargets_.Clear();
    }

    void RegisterImage(RenderImageId imageId, Base::Ref<Texture> texture) noexcept {
        Texture* tex = texture.Get();
        if (lastImageId_ == imageId) lastImage_ = tex;
        imageMap_.Set(imageId, std::move(texture));
    }

    void UnregisterImage(RenderImageId imageId) noexcept {
        if (lastImageId_ == imageId) {
            lastImageId_ = InvalidRenderImageId;
            lastImage_ = nullptr;
        }
        imageMap_.Erase(imageId);
    }

    void RegisterAtlas(std::uint32_t page, Base::Ref<Texture> texture) noexcept {
        atlasMap_.Set(page, std::move(texture));
    }

    void RegisterGlyphRun(RenderGlyphRunId glyphRun, Base::Span<const RenderGlyphQuad> quads) noexcept {
        const auto* found = glyphRunIndex_.FindEntry(glyphRun);
        if (found != nullptr) {
            auto& entry = glyphRuns_[found->Value()];
            entry.quads.Clear();
            entry.quads.Append(quads);
            if (lastGlyphRunId_ == glyphRun) lastGlyphRun_ = &entry.quads;
            return;
        }
        const std::uint32_t idx = glyphRuns_.Size();
        GlyphRunEntry entry;
        entry.glyphRun = glyphRun;
        entry.quads.Append(quads);
        glyphRuns_.PushBack(std::move(entry));
        glyphRunIndex_.Set(glyphRun, idx);
        if (lastGlyphRunId_ == glyphRun) lastGlyphRun_ = &glyphRuns_[idx].quads;
    }

    void UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept {
        if (lastGlyphRunId_ == glyphRun) {
            lastGlyphRunId_ = InvalidRenderGlyphRunId;
            lastGlyphRun_ = nullptr;
        }
        const auto* found = glyphRunIndex_.FindEntry(glyphRun);
        if (found != nullptr) {
            const std::uint32_t i = found->Value();
            glyphRunIndex_.Erase(glyphRun);
            if (i != glyphRuns_.Size() - 1U) {
                glyphRuns_[i] = std::move(glyphRuns_[glyphRuns_.Size() - 1U]);
                glyphRunIndex_.Set(glyphRuns_[i].glyphRun, i);
            }
            static_cast<void>(glyphRuns_.Resize(glyphRuns_.Size() - 1U));
        }
    }

    void RegisterMesh(
        RenderMeshId mesh,
        Base::Span<const Point> vertices,
        Base::Span<const std::uint32_t> indices,
        Base::Span<const float> coverages = {}) noexcept {
        const auto* found = meshIndex_.FindEntry(mesh);
        if (found != nullptr) {
            auto& entry = meshes_[found->Value()];
            entry.vertices.Clear();
            entry.coverages.Clear();
            entry.indices.Clear();
            entry.vertices.Append(vertices);
            if (!coverages.Empty()) entry.coverages.Append(coverages);
            entry.indices.Append(indices);
            if (lastMeshId_ == mesh) lastMesh_ = &entry;
            return;
        }
        const std::uint32_t idx = meshes_.Size();
        MeshEntry entry;
        entry.mesh = mesh;
        entry.vertices.Append(vertices);
        if (!coverages.Empty()) entry.coverages.Append(coverages);
        entry.indices.Append(indices);
        meshes_.PushBack(std::move(entry));
        meshIndex_.Set(mesh, idx);
        if (lastMeshId_ == mesh) lastMesh_ = &meshes_[idx];
    }

    void UnregisterMesh(RenderMeshId mesh) noexcept {
        if (lastMeshId_ == mesh) {
            lastMeshId_ = InvalidRenderMeshId;
            lastMesh_ = nullptr;
        }
        const auto* found = meshIndex_.FindEntry(mesh);
        if (found != nullptr) {
            const std::uint32_t i = found->Value();
            meshIndex_.Erase(mesh);
            if (i != meshes_.Size() - 1U) {
                meshes_[i] = std::move(meshes_[meshes_.Size() - 1U]);
                meshIndex_.Set(meshes_[i].mesh, i);
            }
            static_cast<void>(meshes_.Resize(meshes_.Size() - 1U));
        }
    }

    Texture* FindImage(RenderImageId id) const noexcept {
        if (id == InvalidRenderImageId) return nullptr;
        if (id == lastImageId_) return lastImage_;
        const auto* entry = imageMap_.FindEntry(id);
        if (entry != nullptr) {
            lastImageId_ = id;
            lastImage_ = entry->Value().Get();
            return lastImage_;
        }
        return nullptr;
    }

    Texture* FindAtlas(std::uint32_t page) const noexcept {
        const auto* entry = atlasMap_.FindEntry(page);
        return entry != nullptr ? entry->Value().Get() : nullptr;
    }

    const Base::Vector<RenderGlyphQuad>* FindGlyphRun(RenderGlyphRunId id) const noexcept {
        if (id == InvalidRenderGlyphRunId) return nullptr;
        if (id == lastGlyphRunId_) return lastGlyphRun_;
        const auto* entry = glyphRunIndex_.FindEntry(id);
        if (entry != nullptr) {
            lastGlyphRunId_ = id;
            lastGlyphRun_ = &glyphRuns_[entry->Value()].quads;
            return lastGlyphRun_;
        }
        return nullptr;
    }

    const MeshEntry* FindMesh(RenderMeshId id) const noexcept {
        if (id == InvalidRenderMeshId) return nullptr;
        if (id == lastMeshId_) return lastMesh_;
        const auto* entry = meshIndex_.FindEntry(id);
        if (entry != nullptr) {
            lastMeshId_ = id;
            lastMesh_ = &meshes_[entry->Value()];
            return lastMesh_;
        }
        return nullptr;
    }

    Base::Vector<GradientEntry>& GradientRamps() noexcept { return gradientRamps_; }
    const Base::Vector<GradientEntry>& GradientRamps() const noexcept { return gradientRamps_; }
    Base::Vector<GlyphRunEntry>& GlyphRuns() noexcept { return glyphRuns_; }
    const Base::Vector<GlyphRunEntry>& GlyphRuns() const noexcept { return glyphRuns_; }
    Base::Vector<MeshEntry>& Meshes() noexcept { return meshes_; }
    const Base::Vector<MeshEntry>& Meshes() const noexcept { return meshes_; }
    Base::Vector<OffscreenTargetEntry>& OffscreenTargets() noexcept { return offscreenTargets_; }
    const Base::Vector<OffscreenTargetEntry>& OffscreenTargets() const noexcept { return offscreenTargets_; }

private:
    Base::HashMap<RenderImageId, Base::Ref<Texture>> imageMap_;
    Base::HashMap<std::uint32_t, Base::Ref<Texture>> atlasMap_;
    Base::HashMap<RenderGlyphRunId, std::uint32_t> glyphRunIndex_;
    Base::HashMap<RenderMeshId, std::uint32_t> meshIndex_;
    Base::Vector<GradientEntry> gradientRamps_;
    Base::Vector<GlyphRunEntry> glyphRuns_;
    Base::Vector<MeshEntry> meshes_;
    Base::Vector<OffscreenTargetEntry> offscreenTargets_;

    mutable RenderImageId lastImageId_ = InvalidRenderImageId;
    mutable Texture* lastImage_ = nullptr;
    mutable RenderGlyphRunId lastGlyphRunId_ = InvalidRenderGlyphRunId;
    mutable const Base::Vector<RenderGlyphQuad>* lastGlyphRun_ = nullptr;
    mutable RenderMeshId lastMeshId_ = InvalidRenderMeshId;
    mutable const MeshEntry* lastMesh_ = nullptr;
};

class RenderPassPlanner {
public:
    static bool NeedsOffscreenFBO(
        const RenderNodeSnapshot& node,
        const RenderMaskSnapshot* mask,
        const RenderEffectSnapshot* effect,
        std::uint32_t subtreeCommands,
        bool hasNonAffine3D) noexcept {
        if (node.opacity <= 0.0) return false;
        if (effect != nullptr) {
            if (effect->kind == RenderEffectKind::Blur ||
                effect->kind == RenderEffectKind::DirectionalBlur ||
                effect->kind == RenderEffectKind::Custom ||
                effect->kind == RenderEffectKind::Pixelate ||
                effect->kind == RenderEffectKind::DropShadow) {
                return true;
            }
        }
        if (mask != nullptr && mask->kind != RenderMaskKind::None && mask->kind != RenderMaskKind::Solid) {
            return true;
        }
        if (node.opacity < 1.0 && subtreeCommands > 1U) {
            return true;
        }
        if (node.blendMode != ::Aero::BlendMode::Normal && subtreeCommands > 1U) {
            return true;
        }
        if (hasNonAffine3D) {
            return true;
        }
        return false;
    }
};

struct FrameStatistics {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t vertexCount = 0U;
    std::uint32_t indexCount = 0U;
    std::uint32_t batchCount = 0U;
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
        Base::Span<const std::uint32_t> indices,
        Base::Span<const float> coverages = {}) noexcept;
    void UnregisterMesh(RenderMeshId mesh) noexcept;

    Base::Result<void> RecordOffscreen(
        const RenderFrame& frame) noexcept;

    Base::Result<void> RecordOnscreen(
        const RenderFrame& frame,
        RenderTarget& target) noexcept;

    FrameStatistics LastStatistics() const noexcept { return stats_; }

    bool IsPPAAEnabled() const noexcept { return enablePPAA_; }
    void SetPPAAEnabled(bool enabled) noexcept { enablePPAA_ = enabled; }

    using ImageEntry = RenderResourceTable::ImageEntry;
    using AtlasEntry = RenderResourceTable::AtlasEntry;
    using GradientEntry = RenderResourceTable::GradientEntry;
    using GlyphRunEntry = RenderResourceTable::GlyphRunEntry;
    using MeshEntry = RenderResourceTable::MeshEntry;
    using OffscreenTargetEntry = RenderResourceTable::OffscreenTargetEntry;

    struct ClipEntry {
        Rect rect;
        ProjectiveTransform2D transform;
        bool geometry = false;
        std::uint32_t vertexOffset = 0U;
        std::uint32_t vertexCount = 0U;
        std::uint32_t indexOffset = 0U;
        std::uint32_t indexCount = 0U;
    };

    struct QueuedBatch {
        Batch batch{};
        float pixelUniforms0[8]{};
        float pixelUniforms1[4]{};
        float vertexUniforms0[4]{};
        float vertexUniforms1[4]{};
    };

    void ResetFrame() noexcept;
    void FlushBatch() noexcept;
    void CommitBatches() noexcept;
    bool EnsureMappedBuffers() noexcept;
    Texture* FindImage(RenderImageId id) const noexcept;
    Texture* FindAtlas(std::uint32_t page) const noexcept;
    const Base::Vector<RenderGlyphQuad>* FindGlyphRun(
        RenderGlyphRunId glyphRun) const noexcept;
    const MeshEntry* FindMesh(RenderMeshId mesh) const noexcept;
    Texture* GetOrCreateGradientRamp(const RenderGradientRampSnapshot& ramp) noexcept;
    RenderTarget* GetOrCreateOffscreenTarget(
        RenderNodeId nodeId, std::uint32_t width, std::uint32_t height,
        bool isMask = false) noexcept;
    void BeginOffscreenTargetFrame() noexcept;
    void ReleaseOffscreenTarget(RenderTarget* target) noexcept;

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

    void SetContentStencil() noexcept;

    void EnsureBatchBlend(Shader::Enum shader) noexcept;

    void SetBatchImage(
        Shader::Enum shader,
        Texture* texture,
        Texture* maskTexture = nullptr) noexcept;
    void SetBatchRamp(
        Shader::Enum shader,
        Texture* ramp,
        const float uniforms[8],
        std::uint8_t wrapMode = 0U) noexcept;

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
        double padX,
        double padY,
        double width,
        double height,
        const RenderFrame& frame) noexcept;

    void EmitTriangles(
        const Point* vertices,
        std::uint32_t vertexCount,
        const std::uint32_t* indices,
        std::uint32_t indexCount,
        const ProjectiveTransform2D& transform,
        Color color,
        double opacity) noexcept;

    void EmitAnalyticalShadow(
        const CmdAnalyticalShadow& cmd,
        const ProjectiveTransform2D& transform,
        double opacity) noexcept;

    void ProcessCommand(
        const RenderCommandHeader& h,
        const ProjectiveTransform2D& currentTransform,
        double currentOpacity) noexcept;

    bool EnsureCapacity(std::uint32_t additionalBytes, std::uint32_t additionalIndices) noexcept;
    bool PrepareGeometrySpace(
        std::uint32_t vertexCount,
        std::uint32_t vertexStride,
        std::uint32_t indexCount) noexcept;
    const VertexOffsets& CurrentVertexFormat(uint32_t& outStride) const noexcept;

    RenderBlendMode::Enum currentBlendMode_ = BlendMode::SrcOver;

    RenderDevice* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;

    RenderResourceTable resources_{allocator_};
    Base::Vector<ClipEntry> clipStack_{allocator_};
    struct PushState {
        ProjectiveTransform2D transform;
        double opacity = 1.0;
    };
    Base::Vector<PushState> pushStack_{allocator_};
    float offscreenSizeUniform_[2] = {0.0F, 0.0F};
    float glyphAtlasSizeUniform_[2] = {0.0F, 0.0F};
    float customEffectUniforms_[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    float paintUniforms_[8]{};
    const RenderFrame* currentFrame_ = nullptr;
    Base::Vector<RenderTarget*> pendingReleaseTargets_{allocator_};

    std::uint8_t clipDepth_ = 0U;

    Base::Vector<uint8_t> cpuVertices_{allocator_};
    Base::Vector<uint16_t> cpuIndices_{allocator_};
    uint8_t* mappedVertices_ = nullptr;
    uint16_t* mappedIndices_ = nullptr;
    uint32_t currentVertexOffset_ = 0U;
    uint32_t currentVertexCount_ = 0U;
    uint32_t currentIndexCount_ = 0U;

    Batch currentBatch_{};
    Base::Vector<QueuedBatch> queuedBatches_{allocator_};
    uint32_t currentBatchVertexStart_ = 0U;
    FrameStatistics stats_{};
    bool initialized_ = false;
    bool enablePPAA_ = true;

    Base::Vector<std::uint32_t> recordDepths_{allocator_};
    Base::Vector<RenderNodeId> recordAncestorStack_{allocator_};
    Base::Vector<std::uint32_t> recordSubtreeEnds_{allocator_};
    Base::Vector<std::uint32_t> recordCommandPrefix_{allocator_};
    Base::Vector<std::uint32_t> recordEffectMaskPrefix_{allocator_};
};

using FrameEncoderStatistics = FrameStatistics;

} // namespace Aero::Render

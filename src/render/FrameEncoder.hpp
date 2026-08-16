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

    struct OffscreenTargetEntry {
        RenderNodeId nodeId = InvalidRenderNodeId;
        Ref<RenderTarget> target;
        std::uint32_t width = 0U;
        std::uint32_t height = 0U;
    };

    void ResetFrame() noexcept;
    void FlushBatch() noexcept;
    Texture* FindImage(RenderImageId id) const noexcept;
    Texture* FindAtlas(std::uint32_t page) const noexcept;
    Texture* GetOrCreateGradientRamp(const RenderGradientRampSnapshot& ramp) noexcept;
    RenderTarget* GetOrCreateOffscreenTarget(
        RenderNodeId nodeId, std::uint32_t width, std::uint32_t height) noexcept;

    void EmitQuad(
        const Point points[4],
        const Point uvs[4],
        Color color) noexcept;

    void ProcessCommand(
        const RenderCommand& cmd,
        const Transform2D& currentTransform,
        double currentOpacity) noexcept;

    RenderDevice* device_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;

    Base::Vector<ImageEntry> images_{allocator_};
    Base::Vector<AtlasEntry> atlases_{allocator_};
    Base::Vector<GradientEntry> gradients_{allocator_};
    Base::Vector<OffscreenTargetEntry> offscreenTargets_{allocator_};

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

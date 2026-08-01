#pragma once

#include "DisplayList.hpp"

#include "RenderTree.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Render::Detail {

enum class BatchPipeline : std::uint8_t {
    Rectangle = 0U,
    Image,
    Mesh,
    Glyph,
};

struct DrawPacket final {
    BatchPipeline pipeline = BatchPipeline::Rectangle;
    RenderCommandKind commandKind =
        RenderCommandKind::FillRect;
    RenderNodeId node = InvalidRenderNodeId;
    std::uint64_t resource = 0U;
    std::uint64_t stateSignature = 0U;
    std::uint32_t order = 0U;
};

struct BatchRecord final {
    BatchPipeline pipeline = BatchPipeline::Rectangle;
    std::uint64_t resource = 0U;
    std::uint64_t stateSignature = 0U;
    std::uint32_t firstPacket = 0U;
    std::uint32_t packetCount = 0U;
};

struct BatchPlanStatistics final {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
};

class BatchPlan final {
public:
    explicit BatchPlan(
        Base::IAllocator* allocator = nullptr) noexcept
        : packets_(allocator),
          batches_(allocator) {}

    Base::Span<const DrawPacket> Packets() const noexcept {
        return packets_.AsSpan();
    }
    Base::Span<const BatchRecord> Batches() const noexcept {
        return batches_.AsSpan();
    }
    const BatchPlanStatistics& Statistics() const noexcept {
        return statistics_;
    }

private:
    friend class BatchPlanner;
    Base::Vector<DrawPacket> packets_;
    Base::Vector<BatchRecord> batches_;
    BatchPlanStatistics statistics_;
};

class BatchPlanner final {
public:
    explicit BatchPlanner(
        Base::IAllocator* allocator = nullptr) noexcept
        : allocator_(allocator != nullptr
              ? allocator
              : &Base::GetDefaultAllocator()) {}

    Base::Result<BatchPlan> Build(
        const RenderFrame& plan,
        bool batchingEnabled = true,
        std::uint32_t packetCapacity = 64U) const noexcept;
    Base::Result<BatchPlan> BuildCommandsForTesting(
        Base::Span<const RenderCommand> commands,
        bool batchingEnabled = true,
        std::uint32_t packetCapacity = 64U) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
};

} // namespace Aero::Render::Detail

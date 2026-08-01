#include "BatchPlanner.hpp"

namespace Aero::Render::Detail {
namespace {

bool IsStateCommand(
    RenderCommandKind kind) noexcept {
    switch (kind) {
    case RenderCommandKind::PushClip:
    case RenderCommandKind::PopClip:
    case RenderCommandKind::PushOpacity:
    case RenderCommandKind::PopOpacity:
    case RenderCommandKind::PushTransform:
    case RenderCommandKind::PopTransform:
        return true;
    default:
        return false;
    }
}

Base::Result<DrawPacket> MakePacket(
    const RenderCommand& command,
    RenderNodeId node,
    std::uint64_t stateSignature,
    std::uint32_t order) noexcept {
    DrawPacket packet;
    packet.commandKind = command.kind;
    packet.node = node;
    packet.stateSignature = stateSignature;
    packet.order = order;
    switch (command.kind) {
    case RenderCommandKind::FillRect:
    case RenderCommandKind::FillRoundedRect:
        packet.pipeline = BatchPipeline::Rectangle;
        packet.resource = 0U;
        break;
    case RenderCommandKind::StrokeRect:
        packet.pipeline = BatchPipeline::Rectangle;
        // Stroke uses a distinct instance mode and cannot join fills.
        packet.resource = 1U;
        break;
    case RenderCommandKind::DrawImage:
        packet.pipeline = BatchPipeline::Image;
        packet.resource = command.image;
        break;
    case RenderCommandKind::DrawMesh:
        packet.pipeline = BatchPipeline::Mesh;
        packet.resource = command.mesh;
        break;
    case RenderCommandKind::DrawGlyphRun:
        packet.pipeline = BatchPipeline::Glyph;
        packet.resource = command.glyphRun;
        break;
    default:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "BatchPlanner command is not drawable");
    }
    return packet;
}

bool Compatible(
    const BatchRecord& batch,
    const DrawPacket& packet,
    bool batchingEnabled,
    std::uint32_t capacity) noexcept {
    return batchingEnabled &&
        batch.packetCount < capacity &&
        batch.pipeline == packet.pipeline &&
        batch.resource == packet.resource &&
        batch.stateSignature == packet.stateSignature;
}

} // namespace

Base::Result<BatchPlan> BatchPlanner::Build(
    const RenderFrame& plan,
    bool batchingEnabled,
    std::uint32_t packetCapacity) const noexcept {
    if (packetCapacity == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "BatchPlanner packet capacity must be nonzero");
    }
    BatchPlan output(allocator_);
    output.statistics_.sourceCommandCount =
        plan.Commands().Size();
    std::uint64_t stateSignature = 1U;
    std::uint32_t order = 0U;
    bool hasOpenBatch = false;
    const Base::Span<const RenderCommand> commands =
        plan.Commands();
    for (const RenderNodeSnapshot& node : plan.Nodes()) {
        if (node.commandOffset > commands.Size() ||
            node.commandCount >
                commands.Size() - node.commandOffset) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "BatchPlanner node command range is invalid");
        }
        // The first implementation keeps node boundaries explicit. This is
        // conservative until backend instance payloads carry per-packet
        // transforms and clip signatures.
        if (hasOpenBatch) {
            ++output.statistics_.barrierCount;
            ++stateSignature;
            hasOpenBatch = false;
        }
        for (std::uint32_t index = 0U;
             index < node.commandCount;
             ++index) {
            const RenderCommand& command =
                commands[node.commandOffset + index];
            if (IsStateCommand(command.kind)) {
                ++output.statistics_.barrierCount;
                ++stateSignature;
                hasOpenBatch = false;
                continue;
            }
            Base::Result<DrawPacket> made =
                MakePacket(
                    command,
                    node.id,
                    stateSignature,
                    order++);
            if (!made) return made.GetStatus();
            Base::Result<void> packetAdded =
                output.packets_.TryPushBack(
                    made.Value());
            if (!packetAdded) {
                return packetAdded.GetStatus();
            }
            const DrawPacket& packet =
                output.packets_.Back();
            if (hasOpenBatch &&
                Compatible(
                    output.batches_.Back(),
                    packet,
                    batchingEnabled,
                    packetCapacity)) {
                ++output.batches_.Back().packetCount;
                ++output.statistics_.mergedPacketCount;
                continue;
            }
            BatchRecord batch;
            batch.pipeline = packet.pipeline;
            batch.resource = packet.resource;
            batch.stateSignature =
                packet.stateSignature;
            batch.firstPacket =
                output.packets_.Size() - 1U;
            batch.packetCount = 1U;
            Base::Result<void> batchAdded =
                output.batches_.TryPushBack(batch);
            if (!batchAdded) {
                return batchAdded.GetStatus();
            }
            hasOpenBatch = true;
        }
    }
    output.statistics_.drawPacketCount =
        output.packets_.Size();
    output.statistics_.batchCount =
        output.batches_.Size();
    return output;
}

Base::Result<BatchPlan>
BatchPlanner::BuildCommandsForTesting(
    Base::Span<const RenderCommand> commands,
    bool batchingEnabled,
    std::uint32_t packetCapacity) const noexcept {
    if (packetCapacity == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "BatchPlanner packet capacity must be nonzero");
    }
    BatchPlan output(allocator_);
    output.statistics_.sourceCommandCount =
        commands.Size();
    std::uint64_t stateSignature = 1U;
    std::uint32_t order = 0U;
    bool hasOpenBatch = false;
    for (const RenderCommand& command : commands) {
        if (IsStateCommand(command.kind)) {
            ++output.statistics_.barrierCount;
            ++stateSignature;
            hasOpenBatch = false;
            continue;
        }
        Base::Result<DrawPacket> made =
            MakePacket(
                command,
                1U,
                stateSignature,
                order++);
        if (!made) return made.GetStatus();
        Base::Result<void> packetAdded =
            output.packets_.TryPushBack(
                made.Value());
        if (!packetAdded) {
            return packetAdded.GetStatus();
        }
        const DrawPacket& packet =
            output.packets_.Back();
        if (hasOpenBatch &&
            Compatible(
                output.batches_.Back(),
                packet,
                batchingEnabled,
                packetCapacity)) {
            ++output.batches_.Back().packetCount;
            ++output.statistics_.mergedPacketCount;
            continue;
        }
        BatchRecord batch;
        batch.pipeline = packet.pipeline;
        batch.resource = packet.resource;
        batch.stateSignature =
            packet.stateSignature;
        batch.firstPacket =
            output.packets_.Size() - 1U;
        batch.packetCount = 1U;
        Base::Result<void> batchAdded =
            output.batches_.TryPushBack(batch);
        if (!batchAdded) {
            return batchAdded.GetStatus();
        }
        hasOpenBatch = true;
    }
    output.statistics_.drawPacketCount =
        output.packets_.Size();
    output.statistics_.batchCount =
        output.batches_.Size();
    return output;
}

} // namespace Aero::Render::Detail

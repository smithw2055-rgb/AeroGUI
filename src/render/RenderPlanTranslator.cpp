#include <Aero/Render/RenderPlanTranslator.hpp>

namespace Aero::Render {

Base::Result<Rhi::CommandBuffer> RenderPlanTranslator::Translate(
    const Presentation::RenderPlan& plan) const noexcept {
    const Base::Span<const Presentation::RenderCommand> commands = plan.Commands();
    const Base::Span<const Presentation::RenderNodeSnapshot> nodes = plan.Nodes();
    Rhi::CommandBuffer output(allocator_);
    Base::Result<void> reserved = output.commands_.TryReserve(
        commands.Size() + (nodes.Size() * 2U));
    if (!reserved) return reserved.GetStatus();

    for (const Presentation::RenderNodeSnapshot& node : nodes) {
        if (node.id == Presentation::InvalidRenderNodeId ||
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RenderPlan contains an invalid node command range");
        }

        Rhi::RhiCommand begin;
        begin.kind = Rhi::RhiCommandKind::BeginPass;
        begin.rect = node.clip;
        begin.nodeId = node.id;
        Base::Result<void> appended = output.commands_.TryPushBack(begin);
        if (!appended) return appended.GetStatus();

        for (std::uint32_t index = 0U; index < node.commandCount; ++index) {
            const Presentation::RenderCommand& source =
                commands[node.commandOffset + index];
            Rhi::RhiCommand translated;
            translated.rect = source.rect;
            translated.transform = source.transform;
            translated.color = source.color;
            translated.scalar = source.scalar;
            translated.nodeId = node.id;

            switch (source.kind) {
            case Presentation::RenderCommandKind::PushClip:
                translated.kind = Rhi::RhiCommandKind::PushClip;
                break;
            case Presentation::RenderCommandKind::PopClip:
                translated.kind = Rhi::RhiCommandKind::PopClip;
                break;
            case Presentation::RenderCommandKind::PushOpacity:
                translated.kind = Rhi::RhiCommandKind::PushOpacity;
                break;
            case Presentation::RenderCommandKind::PopOpacity:
                translated.kind = Rhi::RhiCommandKind::PopOpacity;
                break;
            case Presentation::RenderCommandKind::PushTransform:
                translated.kind = Rhi::RhiCommandKind::PushTransform;
                break;
            case Presentation::RenderCommandKind::PopTransform:
                translated.kind = Rhi::RhiCommandKind::PopTransform;
                break;
            case Presentation::RenderCommandKind::FillRect:
                translated.kind = Rhi::RhiCommandKind::DrawFilledRect;
                break;
            case Presentation::RenderCommandKind::FillRoundedRect:
                return Base::Status::Failure(Base::ErrorCode::Unsupported,
                    "Legacy RHI does not support rounded rectangle commands");
            case Presentation::RenderCommandKind::StrokeRect:
                translated.kind = Rhi::RhiCommandKind::DrawStrokedRect;
                break;
            case Presentation::RenderCommandKind::DrawImage:
                return Base::Status::Failure(Base::ErrorCode::Unsupported,
                    "Legacy RHI does not support image commands");
            case Presentation::RenderCommandKind::DrawMesh:
                return Base::Status::Failure(Base::ErrorCode::Unsupported,
                    "Legacy RHI does not support mesh commands");
            case Presentation::RenderCommandKind::DrawGlyphRun:
                return Base::Status::Failure(Base::ErrorCode::Unsupported,
                    "Legacy RHI does not support glyph-run commands");
            }

            appended = output.commands_.TryPushBack(translated);
            if (!appended) return appended.GetStatus();
        }

        Rhi::RhiCommand end;
        end.kind = Rhi::RhiCommandKind::EndPass;
        end.nodeId = node.id;
        appended = output.commands_.TryPushBack(end);
        if (!appended) return appended.GetStatus();
    }

    return output;
}

} // namespace Aero::Render

#include "LoaderResult.hpp"
#include <Aero/Core/Metadata/MetadataRuntime.hpp>

namespace Aero::Markup {

Base::Result<void> VisualContentPlan::TryReserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    Base::Result<void> reserved = contentEdges.TryReserve(contentEdgeCount);
    if (!reserved) return reserved.GetStatus();
    reserved = mountEdges.TryReserve(mountEdgeCount);
    if (!reserved) return reserved.GetStatus();
    return nodes.TryReserve(nodeCount);
}

Base::Result<void> VisualContentPlan::TryAddNode(
    Aero::Visual& node) noexcept {
    for (Aero::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    return nodes.TryPushBack(&node);
}

void VisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        VisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() ==
                    edge.parentOwner.Get() &&
                (!edge.property ||
                 contentEdges[prior].member ==
                     edge.member)) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.runtime != nullptr && edge.parentOwner) {
            if (edge.property) {
                const Core::PropertyInfo* property =
                    edge.runtime->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.runtime->SetProperty(
                        *edge.parentOwner.Get(),
                        edge.member,
                        Core::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.runtime->ClearContent(
                    *edge.parentOwner.Get(),
                    edge.member);
            }
        }
    }
}

void VisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}

} // namespace Aero::Markup

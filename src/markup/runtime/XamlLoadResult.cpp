#include <Aero/Markup/Runtime/XamlLoadResult.hpp>

namespace Aero::Markup {

Base::Result<void> XamlVisualContentPlan::TryReserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    Base::Result<void> reserved = contentEdges.TryReserve(contentEdgeCount);
    if (!reserved) return reserved.GetStatus();
    reserved = mountEdges.TryReserve(mountEdgeCount);
    if (!reserved) return reserved.GetStatus();
    return nodes.TryReserve(nodeCount);
}

Base::Result<void> XamlVisualContentPlan::TryAddNode(
    Presentation::Visual& node) noexcept {
    for (Presentation::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    return nodes.TryPushBack(&node);
}

void XamlVisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        XamlVisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() == edge.parentOwner.Get()) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.clearContent != nullptr && edge.parentOwner) {
            (void)edge.clearContent(
                *edge.parentOwner.Get(), edge.contentContext);
        }
    }
}

void XamlVisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}

} // namespace Aero::Markup

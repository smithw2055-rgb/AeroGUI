#include <Aero/Presentation/VisualTreeMount.hpp>

namespace Aero::Presentation {

VisualTreeMount::VisualTreeMount(
    ObjectTree& tree,
    LayoutManager& layout,
    RenderManager* renderer) noexcept
    : mounts_(tree, &layout, renderer) {}

Base::Status VisualTreeMount::InvalidState(
    const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Result<void> VisualTreeMount::Mount(
    Visual& root,
    UIElement& rootLayout,
    FrameworkElement* rootRender,
    Base::Span<VisualTreeMountEdge> edges,
    Size availableSize) noexcept {
    if (mounted_ || !IsValidLayoutSize(availableSize)) {
        return InvalidState(
            "visual tree mount cannot attach in its current state");
    }

    rootNode_ = &root;
    rootLayout_ = &rootLayout;
    rootRender_ = rootRender;

    Base::Result<MountRootState> rootMounted =
        mounts_.AttachRoot(root, availableSize);
    if (!rootMounted) {
        rootNode_ = nullptr;
        rootLayout_ = nullptr;
        rootRender_ = nullptr;
        return rootMounted.GetStatus();
    }
    rootMount_ = std::move(rootMounted).Value();

    std::uint32_t attached = 0U;
    while (attached < edges.Size()) {
        bool progressed = false;
        for (VisualTreeMountEdge& edge : edges) {
            if (edge.state.logicalAttached || edge.parent == nullptr ||
                edge.child == nullptr ||
                edge.parent->OwningTree() != &mounts_.Tree()) {
                continue;
            }
            Base::Result<MountEdgeState> mounted =
                mounts_.Attach(*edge.parent, *edge.child);
            if (!mounted) {
                (void)Unmount(edges);
                return mounted.GetStatus();
            }
            edge.state = std::move(mounted).Value();
            ++attached;
            progressed = true;
        }
        if (!progressed) {
            (void)Unmount(edges);
            return InvalidState(
                "visual tree mount graph is disconnected from its root");
        }
    }

    mounted_ = true;
    return {};
}

Base::Result<void> VisualTreeMount::Resize(Size availableSize) noexcept {
    if (!mounted_ || rootLayout_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "visual tree resize requires a mounted layout root");
    }
    if (!IsValidLayoutSize(availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "visual tree resize dimensions are invalid");
    }
    Base::Result<void> resized =
        mounts_.Layout()->SetRoot(rootLayout_, availableSize);
    if (!resized) return resized.GetStatus();
    if (mounts_.Renderer() != nullptr && rootRender_ != nullptr) {
        return mounts_.Renderer()->Invalidate(*rootRender_);
    }
    return {};
}

Base::Result<void> VisualTreeMount::Unmount(
    Base::Span<VisualTreeMountEdge> edges) noexcept {
    if (!mounted_ && rootNode_ == nullptr) return {};

    std::uint32_t remaining = 0U;
    for (const VisualTreeMountEdge& edge : edges) {
        if (edge.state.IsAttached()) ++remaining;
    }
    while (remaining > 0U) {
        bool progressed = false;
        for (VisualTreeMountEdge& edge : edges) {
            if (!edge.state.IsAttached()) continue;

            bool hasMountedChild = false;
            for (const VisualTreeMountEdge& candidate : edges) {
                if (candidate.state.IsAttached() &&
                    candidate.parent == edge.child) {
                    hasMountedChild = true;
                    break;
                }
            }
            if (hasMountedChild) continue;

            Base::Result<void> detached = mounts_.Detach(edge.state);
            if (!detached) return detached.GetStatus();
            --remaining;
            progressed = true;
        }
        if (!progressed) {
            return InvalidState(
                "visual tree mount edges cannot be detached leaf-first");
        }
    }

    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);
    if (!rootDetached) return rootDetached.GetStatus();

    rootMount_ = {};
    rootNode_ = nullptr;
    rootLayout_ = nullptr;
    rootRender_ = nullptr;
    mounted_ = false;
    return {};
}

} // namespace Aero::Presentation

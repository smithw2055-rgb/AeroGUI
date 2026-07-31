#include <Aero/Detail/MountService.hpp>

#include "../render/RenderingInternal.hpp"
#include "RuntimeManagers.hpp"

namespace Aero::Detail {

MountService::MountService(
    ObjectTree& tree,
    LayoutManager* layout,
    Render::RenderManager* renderer) noexcept
    : tree_(&tree), layout_(layout), renderer_(renderer) {}

Base::Status MountService::InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Result<void> MountService::AttachLayout(
    Visual& parent,
    Visual& child,
    bool& attached) noexcept {
    UIElement* parentElement = parent.AsUIElement();
    UIElement* childElement = child.AsUIElement();
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return {};
    }
    Base::Result<void> result =
        layout_->Attach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> MountService::AttachRender(
    Visual& parent,
    Visual& child,
    bool& attached) noexcept {
    FrameworkElement* parentElement = parent.AsFrameworkElement();
    FrameworkElement* childElement = child.AsFrameworkElement();
    if (renderer_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return {};
    }
    Base::Result<void> result =
        renderer_->Attach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> MountService::DetachLayout(
    Visual& parent,
    Visual& child,
    bool& attached) noexcept {
    if (!attached) return {};
    UIElement* parentElement = parent.AsUIElement();
    UIElement* childElement = child.AsUIElement();
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return InvalidState("Mounted layout edge has no LayoutManager");
    }
    Base::Result<void> result =
        layout_->Detach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<void> MountService::DetachRender(
    Visual& parent,
    Visual& child,
    bool& attached) noexcept {
    if (!attached) return {};
    FrameworkElement* parentElement = parent.AsFrameworkElement();
    FrameworkElement* childElement = child.AsFrameworkElement();
    if (renderer_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return InvalidState("Mounted render edge has no RenderManager");
    }
    Base::Result<void> result =
        renderer_->Detach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<MountEdgeState> MountService::Attach(
    Visual& logicalParent,
    Visual& visualParent,
    Visual& child) noexcept {
    if (&logicalParent == &child || &visualParent == &child) {
        return InvalidState("Mount cannot attach a node to itself");
    }

    MountEdgeState state;
    state.logicalParent = &logicalParent;
    state.visualParent = &visualParent;
    state.child = &child;

    Base::Result<void> logical =
        tree_->AttachLogical(logicalParent, child);
    if (!logical) return logical.GetStatus();
    state.logicalAttached = true;

    Base::Result<void> visual =
        tree_->AttachVisual(visualParent, child);
    if (!visual) {
        (void)tree_->DetachLogical(logicalParent, child);
        return visual.GetStatus();
    }
    state.visualAttached = true;

    Base::Result<void> layout =
        AttachLayout(visualParent, child, state.layoutAttached);
    if (!layout) {
        (void)tree_->DetachVisual(visualParent, child);
        (void)tree_->DetachLogical(logicalParent, child);
        return layout.GetStatus();
    }

    Base::Result<void> render =
        AttachRender(visualParent, child, state.renderAttached);
    if (!render) {
        (void)DetachLayout(visualParent, child, state.layoutAttached);
        (void)tree_->DetachVisual(visualParent, child);
        (void)tree_->DetachLogical(logicalParent, child);
        return render.GetStatus();
    }

    Base::Result<VisualHandle> handle = tree_->GetHandle(child);
    if (!handle) {
        (void)Detach(state);
        return handle.GetStatus();
    }
    state.childHandle = handle.Value();
    return state;
}

Base::Result<void> MountService::Detach(MountEdgeState& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.logicalParent == nullptr || state.visualParent == nullptr ||
        state.child == nullptr) {
        return InvalidState("Mount edge state is incomplete");
    }

    Visual& logicalParent = *state.logicalParent;
    Visual& visualParent = *state.visualParent;
    Visual& child = *state.child;

    if (state.renderAttached) {
        Base::Result<void> result =
            DetachRender(visualParent, child, state.renderAttached);
        if (!result) return result.GetStatus();
    }
    if (state.layoutAttached) {
        Base::Result<void> result =
            DetachLayout(visualParent, child, state.layoutAttached);
        if (!result) {
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
    }
    if (state.visualAttached) {
        Base::Result<void> result =
            tree_->DetachVisual(visualParent, child);
        if (!result) {
            (void)AttachLayout(visualParent, child, state.layoutAttached);
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.visualAttached = false;
    }
    if (state.logicalAttached) {
        Base::Result<void> result =
            tree_->DetachLogical(logicalParent, child);
        if (!result) {
            Base::Result<void> restored =
                tree_->AttachVisual(visualParent, child);
            if (restored) state.visualAttached = true;
            (void)AttachLayout(visualParent, child, state.layoutAttached);
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.logicalAttached = false;
    }
    state.childHandle = {};
    return {};
}

Base::Result<void> MountService::DetachVisual(
    MountEdgeState& state) noexcept {
    UiMountState visualState;
    visualState.visualParent = state.visualParent;
    visualState.child = state.child;
    visualState.visualAttached = state.visualAttached;
    visualState.layoutAttached = state.layoutAttached;
    visualState.renderAttached = state.renderAttached;
    Base::Result<void> detached = DetachVisual(visualState);
    state.visualAttached = visualState.visualAttached;
    state.layoutAttached = visualState.layoutAttached;
    state.renderAttached = visualState.renderAttached;
    return detached;
}

Base::Result<void> MountService::AttachVisual(
    MountEdgeState& state,
    Visual& newVisualParent) noexcept {
    if (state.child == nullptr || state.visualAttached ||
        state.layoutAttached || state.renderAttached) {
        return InvalidState(
            "Mount edge is not ready for visual attachment");
    }
    Base::Result<UiMountState> attached =
        AttachVisual(newVisualParent, *state.child);
    if (!attached) return attached.GetStatus();
    UiMountState visualState = std::move(attached).Value();
    state.visualParent = visualState.visualParent;
    state.visualAttached = visualState.visualAttached;
    state.layoutAttached = visualState.layoutAttached;
    state.renderAttached = visualState.renderAttached;
    return {};
}

Base::Result<UiMountState> MountService::AttachVisual(
    Visual& visualParent,
    Visual& child) noexcept {
    UiMountState state;
    state.visualParent = &visualParent;
    state.child = &child;

    Base::Result<void> visual =
        tree_->AttachVisual(visualParent, child);
    if (!visual) return visual.GetStatus();
    state.visualAttached = true;

    Base::Result<void> layout =
        AttachLayout(visualParent, child, state.layoutAttached);
    if (!layout) {
        (void)tree_->DetachVisual(visualParent, child);
        return layout.GetStatus();
    }
    Base::Result<void> render =
        AttachRender(visualParent, child, state.renderAttached);
    if (!render) {
        (void)DetachLayout(visualParent, child, state.layoutAttached);
        (void)tree_->DetachVisual(visualParent, child);
        return render.GetStatus();
    }
    if (state.renderAttached && renderer_ != nullptr &&
        child.AsFrameworkElement() != nullptr) {
        auto attachDescendants =
            [&](auto&& self,
                FrameworkElement& parent) noexcept
                -> Base::Result<void> {
            for (FrameworkElement* descendant :
                 parent.RenderChildren()) {
                if (descendant == nullptr) continue;
                Base::Result<void> attached =
                    renderer_->Attach(
                        parent, *descendant);
                if (!attached) {
                    return attached.GetStatus();
                }
                Base::Result<void> nested =
                    self(self, *descendant);
                if (!nested) return nested.GetStatus();
            }
            return {};
        };
        Base::Result<void> descendants =
            attachDescendants(
                attachDescendants,
                *child.AsFrameworkElement());
        if (!descendants) {
            (void)DetachRender(
                visualParent,
                child,
                state.renderAttached);
            (void)DetachLayout(
                visualParent,
                child,
                state.layoutAttached);
            (void)tree_->DetachVisual(
                visualParent, child);
            state.visualAttached = false;
            return descendants.GetStatus();
        }
    }
    return state;
}

Base::Result<void> MountService::DetachVisual(
    UiMountState& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.visualParent == nullptr || state.child == nullptr) {
        return InvalidState("UI mount state is incomplete");
    }
    Visual& parent = *state.visualParent;
    Visual& child = *state.child;

    if (state.renderAttached) {
        Base::Result<void> result =
            DetachRender(parent, child, state.renderAttached);
        if (!result) return result.GetStatus();
    }
    if (state.layoutAttached) {
        Base::Result<void> result =
            DetachLayout(parent, child, state.layoutAttached);
        if (!result) {
            (void)AttachRender(parent, child, state.renderAttached);
            return result.GetStatus();
        }
    }
    if (state.visualAttached) {
        Base::Result<void> result = tree_->DetachVisual(parent, child);
        if (!result) {
            (void)AttachLayout(parent, child, state.layoutAttached);
            (void)AttachRender(parent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.visualAttached = false;
    }
    return {};
}

Base::Result<UiMountState> MountService::ReparentVisual(
    UiMountState& current,
    Visual& newVisualParent) noexcept {
    if (current.child == nullptr || current.visualParent == nullptr) {
        return InvalidState("UI reparent state is incomplete");
    }
    Visual* oldParent = current.visualParent;
    Visual* child = current.child;
    UiMountState oldState = current;

    Base::Result<void> detached = DetachVisual(current);
    if (!detached) return detached.GetStatus();

    Base::Result<UiMountState> attached =
        AttachVisual(newVisualParent, *child);
    if (attached) return attached;

    Base::Result<UiMountState> restored =
        AttachVisual(*oldParent, *child);
    if (restored) current = std::move(restored).Value();
    else current = oldState;
    return attached.GetStatus();
}

Base::Result<MountRootState> MountService::AttachRoot(
    Visual& root,
    Size availableSize) noexcept {
    MountRootState state;
    state.root = &root;
    state.availableSize = availableSize;

    Base::Result<void> tree = tree_->SetRoot(&root);
    if (!tree) return tree.GetStatus();
    state.treeAttached = true;

    UIElement* layoutRoot = root.AsUIElement();
    if (layoutRoot != nullptr) {
        if (layout_ == nullptr) {
            (void)tree_->SetRoot(nullptr);
            return InvalidState("UI root requires a LayoutManager");
        }
        Base::Result<void> layout =
            layout_->SetRoot(layoutRoot, availableSize);
        if (!layout) {
            (void)tree_->SetRoot(nullptr);
            return layout.GetStatus();
        }
        state.layoutAttached = true;
    }

    FrameworkElement* renderRoot = root.AsFrameworkElement();
    if (renderRoot != nullptr && renderer_ != nullptr) {
        Base::Result<void> render = renderer_->SetRoot(renderRoot);
        if (!render) {
            if (state.layoutAttached) (void)layout_->SetRoot(nullptr, {});
            (void)tree_->SetRoot(nullptr);
            return render.GetStatus();
        }
        state.renderAttached = true;
    }
    return state;
}

Base::Result<void> MountService::DetachRoot(
    MountRootState& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.root == nullptr) return InvalidState("Root mount state is incomplete");

    if (state.renderAttached) {
        Base::Result<void> result = renderer_->SetRoot(nullptr);
        if (!result) return result.GetStatus();
        state.renderAttached = false;
    }
    if (state.layoutAttached) {
        Base::Result<void> result = layout_->SetRoot(nullptr, {});
        if (!result) {
            if (state.root->AsFrameworkElement() != nullptr && renderer_ != nullptr) {
                Base::Result<void> restored =
                    renderer_->SetRoot(state.root->AsFrameworkElement());
                if (restored) state.renderAttached = true;
            }
            return result.GetStatus();
        }
        state.layoutAttached = false;
    }
    if (state.treeAttached) {
        Base::Result<void> result = tree_->SetRoot(nullptr);
        if (!result) {
            if (state.root->AsUIElement() != nullptr && layout_ != nullptr) {
                Base::Result<void> restored = layout_->SetRoot(
                    state.root->AsUIElement(), state.availableSize);
                if (restored) state.layoutAttached = true;
            }
            if (state.root->AsFrameworkElement() != nullptr && renderer_ != nullptr) {
                Base::Result<void> restored =
                    renderer_->SetRoot(state.root->AsFrameworkElement());
                if (restored) state.renderAttached = true;
            }
            return result.GetStatus();
        }
        state.treeAttached = false;
    }
    return {};
}

} // namespace Aero::Detail

#include <Aero/Markup/RuntimeServices.hpp>

namespace Aero::Markup {

namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

} // namespace

MountTransactionService::MountTransactionService(
    Presentation::ObjectTree& tree,
    Presentation::LayoutManager& layout,
    Presentation::RenderManager* renderer) noexcept
    : tree_(&tree), layout_(&layout), renderer_(renderer) {}

Base::Result<MountEdgeState>
MountTransactionService::Attach(
    Presentation::Visual& parent,
    Presentation::Visual& child) noexcept {
    if (&parent == &child) {
        return InvalidArgument(
            "Mount edge cannot attach a node to itself");
    }

    MountEdgeState state;
    Base::Result<void> logical =
        tree_->AttachLogical(parent, child);
    if (!logical) {
        return logical.GetStatus();
    }
    state.logicalAttached = true;

    Base::Result<void> visual =
        tree_->AttachVisual(parent, child);
    if (!visual) {
        static_cast<void>(
            tree_->DetachLogical(parent, child));
        return visual.GetStatus();
    }
    state.visualAttached = true;

    Presentation::UIElement* parentElement =
        parent.AsUIElement();
    Presentation::UIElement* childElement =
        child.AsUIElement();
    if ((parentElement == nullptr) !=
        (childElement == nullptr)) {
        static_cast<void>(
            tree_->DetachVisual(parent, child));
        static_cast<void>(
            tree_->DetachLogical(parent, child));
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Mount edge requires matching UIElement participation");
    }
    if (parentElement != nullptr) {
        Base::Result<void> attached =
            layout_->Attach(*parentElement, *childElement);
        if (!attached) {
            static_cast<void>(
                tree_->DetachVisual(parent, child));
            static_cast<void>(
                tree_->DetachLogical(parent, child));
            return attached.GetStatus();
        }
        state.layoutAttached = true;
    }

    Presentation::FrameworkElement* parentFramework =
        parent.AsFrameworkElement();
    Presentation::FrameworkElement* childFramework =
        child.AsFrameworkElement();
    if (renderer_ != nullptr &&
        ((parentFramework == nullptr) !=
         (childFramework == nullptr))) {
        if (state.layoutAttached) {
            static_cast<void>(layout_->Detach(
                *parentElement, *childElement));
        }
        static_cast<void>(
            tree_->DetachVisual(parent, child));
        static_cast<void>(
            tree_->DetachLogical(parent, child));
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Mount edge requires matching FrameworkElement participation");
    }
    if (renderer_ != nullptr &&
        parentFramework != nullptr) {
        Base::Result<void> attached =
            renderer_->Attach(
                *parentFramework, *childFramework);
        if (!attached) {
            if (state.layoutAttached) {
                static_cast<void>(layout_->Detach(
                    *parentElement, *childElement));
            }
            static_cast<void>(
                tree_->DetachVisual(parent, child));
            static_cast<void>(
                tree_->DetachLogical(parent, child));
            return attached.GetStatus();
        }
        state.renderAttached = true;
    }

    Base::Result<Presentation::VisualHandle> handle =
        tree_->GetHandle(child);
    if (!handle) {
        static_cast<void>(Detach(parent, child, &state));
        return handle.GetStatus();
    }
    state.child = handle.Value();
    return state;
}

Base::Result<void> MountTransactionService::Detach(
    Presentation::Visual& parent,
    Presentation::Visual& child,
    MountEdgeState* state) noexcept {
    const bool detachRender = state == nullptr
        ? renderer_ != nullptr &&
            parent.AsFrameworkElement() != nullptr &&
            child.AsFrameworkElement() != nullptr
        : state->renderAttached;
    const bool detachLayout = state == nullptr
        ? parent.AsUIElement() != nullptr &&
            child.AsUIElement() != nullptr
        : state->layoutAttached;
    const bool detachVisual = state == nullptr
        ? child.VisualParent() == &parent
        : state->visualAttached;
    const bool detachLogical = state == nullptr
        ? child.LogicalParent() == &parent
        : state->logicalAttached;

    if (detachRender) {
        Base::Result<void> detached = renderer_->Detach(
            *parent.AsFrameworkElement(),
            *child.AsFrameworkElement());
        if (!detached) return detached.GetStatus();
        if (state != nullptr) state->renderAttached = false;
    }
    if (detachLayout) {
        Base::Result<void> detached = layout_->Detach(
            *parent.AsUIElement(),
            *child.AsUIElement());
        if (!detached) return detached.GetStatus();
        if (state != nullptr) state->layoutAttached = false;
    }
    if (detachVisual) {
        Base::Result<void> detached =
            tree_->DetachVisual(parent, child);
        if (!detached) return detached.GetStatus();
        if (state != nullptr) state->visualAttached = false;
    }
    if (detachLogical) {
        Base::Result<void> detached =
            tree_->DetachLogical(parent, child);
        if (!detached) return detached.GetStatus();
        if (state != nullptr) state->logicalAttached = false;
    }
    if (state != nullptr) state->child = {};
    return {};
}

RuntimeObjectStateStore::RuntimeObjectStateStore(
    Base::IAllocator* allocator) noexcept
    : states_(allocator) {}

RuntimeObjectState* RuntimeObjectStateStore::Find(
    Presentation::VisualHandle handle) noexcept {
    if (!handle.IsValid()) return nullptr;
    for (RuntimeObjectState& state : states_) {
        if (state.handle.index == handle.index &&
            state.handle.generation == handle.generation) {
            return &state;
        }
    }
    return nullptr;
}

const RuntimeObjectState* RuntimeObjectStateStore::Find(
    Presentation::VisualHandle handle) const noexcept {
    if (!handle.IsValid()) return nullptr;
    for (const RuntimeObjectState& state : states_) {
        if (state.handle.index == handle.index &&
            state.handle.generation == handle.generation) {
            return &state;
        }
    }
    return nullptr;
}

Base::Result<RuntimeObjectState*>
RuntimeObjectStateStore::Ensure(
    Presentation::VisualHandle handle) noexcept {
    if (!handle.IsValid()) {
        return InvalidArgument(
            "Runtime sidecar requires a valid generation handle");
    }
    RuntimeObjectState* existing = Find(handle);
    if (existing != nullptr) return existing;
    Base::Result<RuntimeObjectState*> appended =
        states_.TryEmplaceBack();
    if (!appended) return appended.GetStatus();
    appended.Value()->handle = handle;
    return appended.Value();
}

bool RuntimeObjectStateStore::Remove(
    Presentation::VisualHandle handle) noexcept {
    for (std::uint32_t index = 0U;
         index < states_.Size();
         ++index) {
        RuntimeObjectState& state = states_[index];
        if (state.handle.index != handle.index ||
            state.handle.generation != handle.generation) {
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < states_.Size();
             ++next) {
            states_[next - 1U] = states_[next];
        }
        states_.PopBack();
        return true;
    }
    return false;
}

std::uint32_t RuntimeObjectStateStore::Prune(
    const Presentation::ObjectTree& tree) noexcept {
    std::uint32_t removed = 0U;
    for (std::uint32_t index = 0U;
         index < states_.Size();) {
        if (tree.ResolveHandle(states_[index].handle) != nullptr) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < states_.Size();
             ++next) {
            states_[next - 1U] = states_[next];
        }
        states_.PopBack();
        ++removed;
    }
    return removed;
}

} // namespace Aero::Markup

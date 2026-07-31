#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Layout.hpp>
#include <Aero/ObjectTree.hpp>
#include <Aero/Rendering.hpp>

namespace Aero::Detail {

struct MountEdgeState final {
    Visual* logicalParent = nullptr;
    Visual* visualParent = nullptr;
    Visual* child = nullptr;
    VisualHandle childHandle;
    bool logicalAttached = false;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return logicalAttached || visualAttached ||
            layoutAttached || renderAttached;
    }
};

struct UiMountState final {
    Visual* visualParent = nullptr;
    Visual* child = nullptr;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return visualAttached || layoutAttached || renderAttached;
    }
};

struct MountRootState final {
    Visual* root = nullptr;
    Size availableSize;
    bool treeAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return treeAttached || layoutAttached || renderAttached;
    }
};

// Canonical transaction path for logical, visual, layout and render ownership.
// Logical and visual parents are intentionally separate so item generation and
// content projection do not need bespoke attachment sequences. Layout and
// Render participation are optional: an edge joins a subsystem only when the
// service exists and both endpoints expose the corresponding element type.
class AERO_API MountService final {
public:
    MountService(
        ObjectTree& tree,
        LayoutManager* layout = nullptr,
        Render::RenderManager* renderer = nullptr) noexcept;

    Base::Result<MountEdgeState> Attach(
        Visual& parent,
        Visual& child) noexcept {
        return Attach(parent, parent, child);
    }
    Base::Result<MountEdgeState> Attach(
        Visual& logicalParent,
        Visual& visualParent,
        Visual& child) noexcept;
    Base::Result<void> Detach(MountEdgeState& state) noexcept;
    Base::Result<void> DetachVisual(
        MountEdgeState& state) noexcept;
    Base::Result<void> AttachVisual(
        MountEdgeState& state,
        Visual& newVisualParent) noexcept;

    Base::Result<UiMountState> AttachVisual(
        Visual& visualParent,
        Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        UiMountState& state) noexcept;
    Base::Result<UiMountState> ReparentVisual(
        UiMountState& current,
        Visual& newVisualParent) noexcept;

    Base::Result<MountRootState> AttachRoot(
        Visual& root,
        Size availableSize) noexcept;
    Base::Result<void> DetachRoot(MountRootState& state) noexcept;

    ObjectTree& Tree() const noexcept { return *tree_; }
    LayoutManager* Layout() const noexcept { return layout_; }
    Render::RenderManager* Renderer() const noexcept { return renderer_; }

private:
    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    Render::RenderManager* renderer_ = nullptr;

    static Base::Status InvalidState(const char* message) noexcept;
    Base::Result<void> AttachLayout(
        Visual& parent,
        Visual& child,
        bool& attached) noexcept;
    Base::Result<void> AttachRender(
        Visual& parent,
        Visual& child,
        bool& attached) noexcept;
    Base::Result<void> DetachLayout(
        Visual& parent,
        Visual& child,
        bool& attached) noexcept;
    Base::Result<void> DetachRender(
        Visual& parent,
        Visual& child,
        bool& attached) noexcept;
};

} // namespace Aero::Detail

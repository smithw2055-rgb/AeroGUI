#pragma once

#include "../runtime/RuntimeFwd.hpp"

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include "MountService.hpp"

namespace Aero::Detail {

struct VisualTreeMountEdge final {
    UIElement* parent = nullptr;
    UIElement* child = nullptr;
    MountEdgeState state;
};

// Owns UI-side tree attachment for a loaded visual graph. Markup
// decides which content edges exist; the UI runtime owns the long-lived logical,
// visual, layout, render, resize, and detach sequence.
class AERO_API VisualTreeMount final {
public:
    VisualTreeMount(
        ObjectTree& tree,
        LayoutManager& layout,
        Render::RenderTree* renderer = nullptr) noexcept;

    VisualTreeMount(const VisualTreeMount&) = delete;
    VisualTreeMount& operator=(const VisualTreeMount&) = delete;

    Base::Result<void> Mount(
        Visual& root,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<VisualTreeMountEdge> edges,
        Size availableSize) noexcept;
    Base::Result<void> CompleteDeferredEdges(
        Base::Span<VisualTreeMountEdge> edges) noexcept;
    Base::Result<void> Resize(Size availableSize) noexcept;
    Base::Result<void> Unmount(
        Base::Span<VisualTreeMountEdge> edges) noexcept;

    bool IsMounted() const noexcept { return mounted_; }
    ObjectTree& Tree() const noexcept { return mounts_.Tree(); }

private:
    MountService mounts_;
    MountRootState rootMount_;
    Visual* rootNode_ = nullptr;
    UIElement* rootLayout_ = nullptr;
    FrameworkElement* rootRender_ = nullptr;
    bool mounted_ = false;

    static Base::Status InvalidState(const char* message) noexcept;
};

} // namespace Aero::Detail

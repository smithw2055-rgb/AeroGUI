#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include "gui/media/MediaState.hpp"
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace Aero {

std::uint32_t Media::Visual::Access::PanelChildCount(
    const Controls::Panel& panel) noexcept {
    return panel.ChildCountCore();
}

Base::Ref<Base::Object> Media::Visual::Access::PanelChildAt(
    const Controls::Panel& panel,
    std::uint32_t index) noexcept {
    return panel.ChildAtCore(index);
}

Base::Result<void> Media::Visual::Access::PanelAddChild(
    Controls::Panel& panel,
    const Base::Ref<Base::Object>& owner,
    UIElement& child) noexcept {
    return panel.AddChildCore(owner, child);
}

Base::Result<bool> Media::Visual::Access::PanelRemoveChild(
    Controls::Panel& panel,
    UIElement& child) noexcept {
    return panel.RemoveChildCore(child);
}

void Media::Visual::Access::PanelClearChildren(
    Controls::Panel& panel) noexcept {
    panel.ClearChildrenCore();
}

const Base::Ref<Base::Object>& Media::Visual::Access::DecoratorOwnedChild(
    const Controls::Decorator& decorator) noexcept {
    return decorator.ownedChild_;
}

Base::Result<void> Media::Visual::Access::DecoratorSetOwnedChild(
    Controls::Decorator& decorator,
    const Base::Ref<Base::Object>& owner,
    UIElement& child) noexcept {
    decorator.SetOwnedChild(owner, child);
    return {};
}

void Media::Visual::Access::PathInvalidateGeometry(
    Shapes::Path& path) noexcept {
    path.ResetGeometry();
}

void Media::Visual::Access::PathAttachMeshResources(
    Shapes::Path& path,
    void* services,
    bool invalidate) noexcept {
    path.AttachMeshResources(services, invalidate);
    if (invalidate) {
        static_cast<void>(path.InvalidateVisual());
    }
}

void Media::Visual::Access::SetMenuItemHighlighted(
    Controls::MenuItem& item,
    bool value) noexcept {
    item.SetHighlightedState(value);
}

void Media::Visual::Access::SyncSelectorContainers(
    Controls::Primitives::Selector& selector) noexcept {
    selector.SyncContainers();
}

std::uint32_t Media::Visual::Access::TreeViewItemCount(
    const Controls::TreeViewItem& item) noexcept {
    return item.GetCount();
}

} // namespace Aero

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;



void Control::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    // A templated Control delegates its chrome to the template. Painting the
    // base Background as well produces an extra full-control rectangle behind
    // custom ComboBox, TreeView, Button and similar templates.
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}




































































































































} // namespace Aero::Controls

namespace Aero::Controls {









} // namespace Aero::Controls


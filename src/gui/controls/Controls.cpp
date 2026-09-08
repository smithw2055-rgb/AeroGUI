#include "gui/meta/MetadataState.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include "gui/media/MediaHelpers.hpp"
#include <Aero/Collections.hpp>
#include <Aero/Documents.hpp>
#include <Aero/TryCast.hpp>
#include "RichText.hpp"


#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace Aero {

void AeroGuiInternal::PathInvalidateGeometry(
    Shapes::Path& path) noexcept {
    AERO_CALL_METHOD0(path, Path_ResetGeometry);
}

void AeroGuiInternal::PathAttachMeshResources(
    Shapes::Path& path,
    void* services,
    bool invalidate) noexcept {
    AERO_CALL_METHOD(path, Path_AttachMeshResources, services, invalidate);
    if (invalidate) {
        path.InvalidateVisual();
    }
}

void AeroGuiInternal::SetMenuItemHighlighted(
    Controls::MenuItem& item,
    bool value) noexcept {
    AERO_CALL_METHOD(item, MenuItem_SetHighlightedState, value);
}

void AeroGuiInternal::SyncSelectorContainers(
    Controls::Primitives::Selector& selector) noexcept {
    AERO_CALL_METHOD0(selector, Selector_SyncContainers);
}

std::uint32_t AeroGuiInternal::TreeViewItemCount(
    const Controls::TreeViewItem& item) noexcept {
    return item.GetCount();
}

void AeroGuiInternal::OnContentControlPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
    AERO_CALL_STATIC_METHOD(ContentControl_OnContentPropertyChanged, object, change);
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    AERO_CALL_METHOD(control, ItemsControl_SetItemsSourceCore, source);
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Base::Ref<Base::Object> source) noexcept {
    Collections::IItemsSource* directSource =
        TryCastToInterface<Collections::IItemsSource>(source.Get());
    if (directSource == nullptr) {
        directSource = Collections::CollectionAsItemsSource(source.Get());
    }
    AERO_CALL_METHOD(control, ItemsControl_SetItemsSourceCore, directSource);
}

void AeroGuiInternal::SetItemsSourceBorrowed(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetValue(
        Controls::ItemsControl::ItemsSourceProperty,
        Base::Ref<Base::Object>{});
    AERO_CALL_METHOD(control, ItemsControl_SetItemsSourceCore, source);
}

void AeroGuiInternal::SetItemTemplate(
    Controls::ItemsControl& control,
    const DataTemplate* value) noexcept {
    AERO_CALL_METHOD(control, ItemsControl_SetItemTemplateCore, value);
}

void AeroGuiInternal::SetItemTemplateSelector(
    Controls::ItemsControl& control,
    const DataTemplateSelector* value) noexcept {
    AERO_CALL_METHOD(control, ItemsControl_SetItemTemplateSelectorCore, value);
}

void AeroGuiInternal::SetItemsPanel(
    Controls::ItemsControl& control,
    const Controls::ItemsPanelTemplate* value) noexcept {
    AERO_CALL_METHOD(control, ItemsControl_SetItemsPanelCore, value);
}

void AeroGuiInternal::SetItemContainerStyle(
    Controls::ItemsControl& control,
    const Style* value) noexcept {
    AERO_CALL_METHOD(control, ItemsControl_SetItemContainerStyleCore, value);
}

void AeroGuiInternal::RefreshDisplayMemberPath(
    Controls::ItemsControl& control) noexcept {
    AERO_CALL_METHOD0(control, ItemsControl_PublishReset);
}

void AeroGuiInternal::AttachTextLayout(
    Controls::TextBlock& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void AeroGuiInternal::AttachTextLayout(
    Controls::TextBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void AeroGuiInternal::AttachTextLayout(
    Controls::PasswordBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
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
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
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


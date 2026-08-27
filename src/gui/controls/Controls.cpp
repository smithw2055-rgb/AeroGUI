#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
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
#include "gui/media/MediaState.hpp"
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
    path.ResetGeometry();
}

void AeroGuiInternal::PathAttachMeshResources(
    Shapes::Path& path,
    void* services,
    bool invalidate) noexcept {
    path.AttachMeshResources(services, invalidate);
    if (invalidate) {
        static_cast<void>(path.InvalidateVisual());
    }
}

void AeroGuiInternal::SetMenuItemHighlighted(
    Controls::MenuItem& item,
    bool value) noexcept {
    item.SetHighlightedState(value);
}

void AeroGuiInternal::SyncSelectorContainers(
    Controls::Primitives::Selector& selector) noexcept {
    selector.SyncContainers();
}

std::uint32_t AeroGuiInternal::TreeViewItemCount(
    const Controls::TreeViewItem& item) noexcept {
    return item.GetCount();
}

void AeroGuiInternal::OnContentControlPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
    Controls::ContentControl::OnContentPropertyChanged(object, change);
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetItemsSourceCore(source);
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Base::Ref<Base::Object> source) noexcept {
    Collections::IItemsSource* directSource =
        TryCastToInterface<Collections::IItemsSource>(source.Get());
    if (directSource == nullptr) {
        directSource = Collections::CollectionAsItemsSource(source.Get());
    }
    control.SetItemsSourceCore(directSource);
}

void AeroGuiInternal::SetItemsSourceBorrowed(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetValue(
        Controls::ItemsControl::ItemsSourceProperty,
        Base::Ref<Base::Object>{});
    control.SetItemsSourceCore(source);
}

void AeroGuiInternal::SetItemTemplate(
    Controls::ItemsControl& control,
    const DataTemplate* value) noexcept {
    control.SetItemTemplateCore(value);
}

void AeroGuiInternal::SetItemTemplateSelector(
    Controls::ItemsControl& control,
    const DataTemplateSelector* value) noexcept {
    control.SetItemTemplateSelectorCore(value);
}

void AeroGuiInternal::SetItemsPanel(
    Controls::ItemsControl& control,
    const Controls::ItemsPanelTemplate* value) noexcept {
    control.SetItemsPanelCore(value);
}

void AeroGuiInternal::SetItemContainerStyle(
    Controls::ItemsControl& control,
    const Style* value) noexcept {
    control.SetItemContainerStyleCore(value);
}

void AeroGuiInternal::RefreshDisplayMemberPath(
    Controls::ItemsControl& control) noexcept {
    control.PublishReset();
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


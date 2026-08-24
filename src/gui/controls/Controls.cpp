#include "gui/meta/MetadataState.hpp"
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
#include <Aero/Collections.hpp>
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "gui/core/facets/VisualFacet.hpp"
#include "gui/core/facets/DependencyPropertyFacet.hpp"
#include "gui/core/facets/InteractionStateFacet.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace Aero::Core {

std::uint32_t VisualFacet::PanelChildCount(
    const Controls::Panel& panel) noexcept {
    return panel.ChildCountCore();
}

Base::Ref<Base::Object> VisualFacet::PanelChildAt(
    const Controls::Panel& panel,
    std::uint32_t index) noexcept {
    return panel.ChildAtCore(index);
}

Base::Result<void> VisualFacet::PanelAddChild(
    Controls::Panel& panel,
    const Base::Ref<Base::Object>& owner,
    UIElement& child) noexcept {
    return panel.AddChildCore(owner, child);
}

Base::Result<bool> VisualFacet::PanelRemoveChild(
    Controls::Panel& panel,
    UIElement& child) noexcept {
    return panel.RemoveChildCore(child);
}

void VisualFacet::PanelClearChildren(
    Controls::Panel& panel) noexcept {
    panel.ClearChildrenCore();
}

const Base::Ref<Base::Object>& VisualFacet::DecoratorOwnedChild(
    const Controls::Decorator& decorator) noexcept {
    return decorator.ownedChild_;
}

Base::Result<void> VisualFacet::DecoratorSetOwnedChild(
    Controls::Decorator& decorator,
    const Base::Ref<Base::Object>& owner,
    UIElement& child) noexcept {
    decorator.SetOwnedChild(owner, child);
    return {};
}

void DependencyPropertyFacet::PathInvalidateGeometry(
    Shapes::Path& path) noexcept {
    path.ResetGeometry();
}

void DependencyPropertyFacet::PathAttachMeshResources(
    Shapes::Path& path,
    void* services,
    bool invalidate) noexcept {
    path.AttachMeshResources(services, invalidate);
    if (invalidate) {
        static_cast<void>(path.InvalidateVisual());
    }
}

#include "gui/core/facets/TextLayoutFacet.hpp"
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>

void InteractionStateFacet::SetMenuItemHighlighted(
    Controls::MenuItem& item,
    bool value) noexcept {
    item.SetHighlightedState(value);
}

void InteractionStateFacet::SyncSelectorContainers(
    Controls::Primitives::Selector& selector) noexcept {
    selector.SyncContainers();
}

std::uint32_t InteractionStateFacet::TreeViewItemCount(
    const Controls::TreeViewItem& item) noexcept {
    return item.GetCount();
}

bool InteractionStateFacet::IsTemplateApplied(const Controls::Control& control) noexcept {
    return control.templateHandleValue_ != 0U;
}

std::uint64_t InteractionStateFacet::TemplateGeneration(const Controls::Control& control) noexcept {
    return control.templateGeneration_;
}

UIElement* InteractionStateFacet::TemplateRoot(const Controls::Control& control) noexcept {
    return control.templateChild_;
}

Base::Result<void> InteractionStateFacet::SetTemplateRoot(Controls::Control& control, UIElement* child) noexcept {
    control.SetTemplateChildCore(child);
    return {};
}

void InteractionStateFacet::AttachTemplateEngine(Controls::Control& control, void* engine) noexcept {
    static_cast<void>(control);
    static_cast<void>(engine);
}

void InteractionStateFacet::SetVisualStateManager(Controls::Control& control, Aero::VisualStateManager* manager) noexcept {
    static_cast<void>(control);
    static_cast<void>(manager);
}

void InteractionStateFacet::NotifyTemplateApplied(Controls::Control& control, std::uint64_t handleValue) noexcept {
    control.NotifyTemplateApplied(handleValue);
}

void InteractionStateFacet::NotifyTemplateDetached(Controls::Control& control) noexcept {
    control.NotifyTemplateDetached();
}

void InteractionStateFacet::InvokeTemplateApplied(Controls::Control& control) noexcept {
    control.OnApplyTemplate();
}

UIElement* InteractionStateFacet::ContentElement(const Controls::ContentControl& control) noexcept {
    return control.content_;
}

const Base::Ref<Base::Object>& InteractionStateFacet::OwnedContent(const Controls::ContentControl& control) noexcept {
    return control.ownedContent_;
}

const Base::Ref<Base::Object>& InteractionStateFacet::ContentValue(const Controls::ContentControl& control) noexcept {
    return control.contentValue_;
}

Base::Result<void> InteractionStateFacet::SetOwnedContent(
    Controls::ContentControl& control,
    const Base::Ref<Base::Object>& owner,
    UIElement& content) noexcept {
    control.SetOwnedContent(owner, content);
    return {};
}

Base::Result<void> InteractionStateFacet::SetGeneratedTextContent(
    Controls::ContentControl& container,
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    container.SetGeneratedTextContent(contentObject, content);
    return {};
}

Base::Result<void> InteractionStateFacet::SetContentValue(
    Controls::ContentControl& control,
    Base::Ref<Base::Object> value) noexcept {
    control.SetContentValue(std::move(value));
    return {};
}

Base::Result<void> InteractionStateFacet::SetContentValue(
    Controls::ContentControl& control,
    Meta::Value value) noexcept {
    control.SetContentValue(std::move(value));
    return {};
}

void InteractionStateFacet::OnContentControlPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
    Controls::ContentControl::OnContentPropertyChanged(object, change);
}

bool InteractionStateFacet::HasAttachedGenerator(const Controls::ItemsControl& control) noexcept {
    return control.generator_ != nullptr;
}

void InteractionStateFacet::SetItemsSource(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetItemsSourceCore(source);
}

void InteractionStateFacet::SetItemsSource(
    Controls::ItemsControl& control,
    Base::Ref<Base::Object> source) noexcept {
    Collections::IItemsSource* directSource = nullptr;
    if (source) {
        if (source->RuntimeType() == Collections::ObservableCollection::StaticTypeId() ||
            control.PropertyRegistry().Types().IsDerivedFrom(
                source->RuntimeType(),
                Collections::ObservableCollection::StaticTypeId())) {
            directSource = static_cast<Collections::ObservableCollection*>(source.Get());
        }
    }
    control.SetItemsSourceCore(directSource);
}

void InteractionStateFacet::SetItemsSourceBorrowed(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetValue(
        Controls::ItemsControl::ItemsSourceProperty,
        Base::Ref<Base::Object>{});
    control.SetItemsSourceCore(source);
}

void InteractionStateFacet::SetItemTemplate(
    Controls::ItemsControl& control,
    const DataTemplate* value) noexcept {
    control.SetItemTemplateCore(value);
}

void InteractionStateFacet::SetItemsPanel(
    Controls::ItemsControl& control,
    const Controls::ItemsPanelTemplate* value) noexcept {
    control.SetItemsPanelCore(value);
}

void InteractionStateFacet::SetItemContainerStyle(
    Controls::ItemsControl& control,
    const Style* value) noexcept {
    control.SetItemContainerStyleCore(value);
}

void InteractionStateFacet::RefreshDisplayMemberPath(
    Controls::ItemsControl& control) noexcept {
    control.PublishReset();
}

void TextLayoutFacet::AttachTextLayout(
    Controls::TextBlock& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void TextLayoutFacet::AttachTextLayout(
    Controls::TextBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void TextLayoutFacet::AttachTextLayout(
    Controls::PasswordBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

} // namespace Aero::Core

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


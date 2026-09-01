#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/Primitives/ToggleButton.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>

#include <utility>
#include "ControlBehavior.hpp"
#include <Aero/Media/ScaleTransform.hpp>
#include <Aero/Media/RotateTransform.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include "gui/internal/AeroGuiInternal.hpp"

namespace Aero::Controls {
using Aero::Controls::TreeBehavior;

using namespace Primitives;

namespace {

Collections::IItemsSource* AsItemsSource(
    Base::Object* source) noexcept {
    Collections::IItemsSource* items =
        TryCastToInterface<Collections::IItemsSource>(source);
    if (items == nullptr) {
        items = Collections::CollectionAsItemsSource(source);
    }
    return items;
}

void AttachOwnedUiSubtree(
    ElementTree& tree,
    UIElement& parent) noexcept {
    const auto attachChild = [&](UIElement& child) noexcept {
        if (child.GetVisualParent() == &parent &&
            child.GetTree() == &tree &&
            child.GetIsLayoutAttached()) {
            AttachOwnedUiSubtree(tree, child);
            return;
        }
        if (child.GetVisualParent() != nullptr &&
            child.GetVisualParent() != &parent) {
            static_cast<void>(tree.DetachVisual(
                *child.GetVisualParent(),
                static_cast<::Aero::Media::Visual&>(child)));
        }
        if (child.GetTree() == nullptr &&
            child.GetLogicalParent() == nullptr) {
            static_cast<void>(tree.AttachElement(parent, child));
        } else if (child.GetVisualParent() != &parent ||
                   !child.GetIsLayoutAttached()) {
            static_cast<void>(tree.AttachVisualChild(parent, child));
        }
        if (Aero::BindingEngine* bindings =
                AeroGuiInternal::BindingEngineOf(child)) {
            static_cast<void>(bindings->ActivateDeferredWhenReady(child));
        }
        AttachOwnedUiSubtree(tree, child);
    };

    if (parent.PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Panel::StaticTypeId())) {
        auto& panel = static_cast<Panel&>(parent);
        const std::uint32_t count = AeroGuiInternal::PanelChildCount(panel);
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Base::Ref<Base::Object> owned =
                AeroGuiInternal::PanelChildAt(panel, index);
            if (!owned ||
                !parent.PropertyRegistry().Types().IsDerivedFrom(
                    owned->RuntimeType(), UIElement::StaticTypeId())) {
                continue;
            }
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (parent.PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Decorator::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            AeroGuiInternal::DecoratorOwnedChild(
                static_cast<Decorator&>(parent));
        if (owned &&
            parent.PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (parent.PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<ContentPresenter&>(parent);
        const Base::Ref<Base::Object>& owned = presenter.GetOwnedContent();
        if (owned &&
            parent.PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (parent.PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), ContentControl::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            AeroGuiInternal::OwnedContent(
                static_cast<ContentControl&>(parent));
        if (owned &&
            parent.PropertyRegistry().Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
    }
}

void HostHeaderVisual(
    ElementTree&,
    ContentPresenter& presenter,
    const Base::Ref<Base::Object>& owner,
    UIElement& element) noexcept {
    presenter.HostUiElement(owner, element);
}

Base::Ref<Base::Object> DataItemFromContainer(
    TreeViewItem& item) noexcept {
    Base::Ref<Base::Object> context;
    const Value dc = item.GetDataContext();
    if (dc.Kind() == ValueKind::Object &&
        !dc.IsNullObject() &&
        dc.AsObject() &&
        dc.AsObject().Get() != &item) {
        context = dc.AsObject();
    }
    ::Aero::Media::Visual* visual = item.GetVisualParent();
    while (visual != nullptr) {
        UIElement* element = ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            item.PropertyRegistry().Types().IsDerivedFrom(
                element->RuntimeType(),
                ItemsControl::StaticTypeId())) {
            auto& items = static_cast<ItemsControl&>(*element);
            ItemContainerGenerator* generator =
                items.GetItemContainerGenerator();
            if (generator != nullptr) {
                Base::Ref<Base::Object> data =
                    generator->ItemFromContainer(item);
                if (data && data.Get() != &item) {
                    return data;
                }
            }
        }
        visual = visual->GetVisualParent();
    }
    if (context) {
        return context;
    }
    return Base::Ref<Base::Object>::FromBorrowed(item);
}

void UnselectOtherTreeViewItems(
    ::Aero::Media::Visual& parent,
    TreeViewItem* keep,
    Aero::VisualStateManager* states) noexcept {
    for (::Aero::Media::Visual* child :
         AeroGuiInternal::RenderChildren(parent)) {
        if (child == nullptr) continue;
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(child);
        if (element != nullptr &&
            element->PropertyRegistry().Types().IsDerivedFrom(
                element->RuntimeType(),
                TreeViewItem::StaticTypeId())) {
            auto* node = static_cast<TreeViewItem*>(element);
            if (node != keep && node->GetIsSelected()) {
                node->SetIsSelected(false);
                if (states != nullptr) {
                    static_cast<void>(
                        Aero::VisualStateManagerRuntime::GoToState(
                            *states,
                            *node,
                            "SelectionStates",
                            "Unselected"));
                }
            }
        }
        UnselectOtherTreeViewItems(*child, keep, states);
    }
}

} // namespace

TreeViewItem::TreeViewItem() noexcept
    : TreeViewItem(StaticTypeId()) {}

TreeViewItem::TreeViewItem(
    TypeId runtimeType) noexcept
    : HeaderedItemsControl(runtimeType),
      headerChangedHandler_(
          this, &TreeViewItem::OnHeaderChanged),
      iconChangedHandler_(
          this, &TreeViewItem::OnHeaderChanged),
      expandedChangedHandler_(
          this, &TreeViewItem::OnExpandedChanged),
      selectedChangedHandler_(
          this, &TreeViewItem::OnSelectedChanged),
      expandClickHandler_(
          this, &TreeViewItem::OnExpandButtonClick) {
    static_cast<void>(AddValueChangedHandlerChecked(
        HeaderProperty, headerChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IconProperty, iconChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsExpandedProperty,
        expandedChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsSelectedProperty,
        selectedChangedHandler_));
}

TreeViewItem::~TreeViewItem() {
    if (childItems_ != nullptr) {
        AeroGuiInternal::SetItemsSourceBorrowed(
            *childItems_, nullptr);
    }
    if (expandButton_ != nullptr) {
        static_cast<void>(expandButton_->RemoveHandler(
            ButtonBase::ClickEvent, expandClickHandler_));
        expandButton_ = nullptr;
    }
    static_cast<void>(RemoveValueChangedHandler(
        HeaderProperty, headerChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IconProperty, iconChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsExpandedProperty,
        expandedChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsSelectedProperty,
        selectedChangedHandler_));
}

Value
TreeViewItem::GetHeader() const noexcept {
    return GetValueOr(
        HeaderProperty,
        Value::NullObject(Meta::TypeOf<Base::Object>()));
}

void TreeViewItem::SetHeader(
    Value value) noexcept {
    SetValue(HeaderProperty, std::move(value));
}

Base::Result<void> TreeViewItem::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) return boxed.GetStatus();
    SetHeader(std::move(boxed).Value());
    return {};
}

Base::StringView TreeViewItem::GetIcon() const noexcept {
    return GetValueOr(
        IconProperty, Base::StringView{});
}

void TreeViewItem::SetIcon(
    Base::StringView value) noexcept {
    SetValue(IconProperty, value);
}

Base::Ref<DataTemplate>
TreeViewItem::GetHeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

void
TreeViewItem::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(HeaderTemplateProperty, std::move(value));
}

bool TreeViewItem::GetIsExpanded() const noexcept {
    return GetValueOr(
        IsExpandedProperty, false);
}

void
TreeViewItem::SetIsExpanded(
    bool value) noexcept {
    SetCurrentValue(IsExpandedProperty, value);
}

bool TreeViewItem::GetIsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

void
TreeViewItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

Base::Result<Base::Ref<FrameworkElement>>
TreeViewItem::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<TreeViewItem>> made =
        Base::MakeRef<TreeViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

void TreeViewItem::SetHierarchicalContent(
    Base::Ref<Base::Object> source,
    Base::Ref<DataTemplate> itemTemplate) noexcept {
    hierarchicalItemsSource_ = std::move(source);
    hierarchicalItemsBinding_.Reset();
    hierarchicalBindingSource_.Reset();
    hierarchicalItemTemplate_ = std::move(itemTemplate);
auto* items =
        AsItemsSource(hierarchicalItemsSource_.Get());
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemsControl::HasItemsProperty,
        items != nullptr && items->GetCount() != 0U));
    if (GetIsExpanded()) ActivateHierarchicalContent();
}

void TreeViewItem::SetHierarchicalBinding(
    Base::Ref<Data::Binding> binding,
    Base::Ref<Base::Object> source,
    Base::Ref<DataTemplate> itemTemplate) noexcept {
    hierarchicalItemsSource_.Reset();
    hierarchicalItemsBinding_ = std::move(binding);
    hierarchicalBindingSource_ = std::move(source);
    hierarchicalItemTemplate_ = std::move(itemTemplate);
    // A hierarchical declaration represents a potentially expandable node.
    // Resolve the collection lazily on expansion so collapsed trees do not
    // realize and attach every descendant during their parent's binding wave.
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemsControl::HasItemsProperty,
        hierarchicalItemsBinding_ && hierarchicalBindingSource_));
    if (GetIsExpanded()) ActivateHierarchicalContent();
}

void TreeViewItem::ClearHierarchicalContent() noexcept {
    hierarchicalItemsSource_.Reset();
    hierarchicalItemsBinding_.Reset();
    hierarchicalBindingSource_.Reset();
    hierarchicalItemTemplate_.Reset();
}

void TreeViewItem::ActivateHierarchicalContent() noexcept {
    SetItemTemplate(hierarchicalItemTemplate_);
    if (hierarchicalItemsSource_) {
        SetItemsSource(hierarchicalItemsSource_);
    } else if (hierarchicalItemsBinding_ && hierarchicalBindingSource_) {
        auto* bindings = AeroGuiInternal::BindingEngineOf(*this);
        if (bindings == nullptr || bindings->Metadata() == nullptr) return;
        Data::MetadataBindingDescriptor descriptor;
        descriptor.metadata = bindings->Metadata();
        descriptor.source = hierarchicalBindingSource_.Get();
        descriptor.target = this;
        descriptor.targetProperty = ItemsControl::ItemsSourceProperty.Handle();
        descriptor.path = hierarchicalItemsBinding_->GetPathText();
        descriptor.stringFormat =
            hierarchicalItemsBinding_->GetStringFormat();
        descriptor.mode = Data::BindingMode::OneWay;
        descriptor.updateSourceTrigger =
            bindings->ResolveUpdateSourceTrigger(
                *this,
                ItemsControl::ItemsSourceProperty.Handle(),
                hierarchicalItemsBinding_->GetUpdateSourceTrigger());
        descriptor.converterResource =
            hierarchicalItemsBinding_->GetConverter();
        descriptor.converterParameter =
            hierarchicalItemsBinding_->GetConverterParameter();
        descriptor.fallbackValue =
            hierarchicalItemsBinding_->GetFallbackValue();
        descriptor.targetNullValue =
            hierarchicalItemsBinding_->GetTargetNullValue();
        Base::Result<void> queued = bindings->QueueDeferred(descriptor);
        if (!queued) {
            bindings->RecordError(queued.GetStatus());
            return;
        }
        Base::Result<void> activated =
            bindings->ActivateDeferredWhenReady(*this);
        if (!activated) {
            bindings->RecordError(activated.GetStatus());
            return;
        }
    } else {
        return;
    }
    if (childItems_ != nullptr) {
        const Base::Ref<Base::Object> current = GetItemsSource();
auto* items = AsItemsSource(current.Get());
        AeroGuiInternal::SetItemsSourceBorrowed(
            *childItems_, items);
    }
    ProjectRealizedHeaders();
}

void
TreeViewItem::OnApplyTemplate() noexcept {
    HeaderedItemsControl::OnApplyTemplate();

    DependencyObject* header =
        GetTemplateChild("HeaderText");
    headerText_ =
        header != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            header->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(header)
        : nullptr;
    DependencyObject* icon =
        GetTemplateChild("IconText");
    iconText_ =
        icon != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            icon->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(icon)
        : nullptr;
    DependencyObject* glyph =
        GetTemplateChild("ExpanderGlyph");
    expanderGlyph_ =
        glyph != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            glyph->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(glyph)
        : nullptr;
    DependencyObject* children =
        GetTemplateChild("ChildItems");
    childItems_ =
        children != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            children->RuntimeType(),
            ItemsControl::StaticTypeId())
        ? static_cast<ItemsControl*>(children)
        : nullptr;
    DependencyObject* itemsBorder =
        GetTemplateChild("ItemsBorder");
    itemsBorder_ =
        itemsBorder != nullptr
        ? ::Aero::TryCast<::Aero::UIElement>(itemsBorder)
        : nullptr;
    DependencyObject* itemsHost =
        GetTemplateChild("ItemsHost");
    itemsHostPresenter_ =
        itemsHost != nullptr
        ? ::Aero::TryCast<::Aero::UIElement>(itemsHost)
        : nullptr;
    if (expandButton_ != nullptr) {
        static_cast<void>(expandButton_->RemoveHandler(
            ButtonBase::ClickEvent, expandClickHandler_));
        expandButton_ = nullptr;
    }
    DependencyObject* expander =
        GetTemplateChild("ExpandButton");
    expandButton_ =
        expander != nullptr
        ? ::Aero::TryCast<ToggleButton>(expander)
        : nullptr;
    if (expandButton_ != nullptr) {
        static_cast<void>(expandButton_->AddHandlerChecked(
            ButtonBase::ClickEvent, expandClickHandler_));
    }

    if (childItems_ != nullptr) {
        Base::Ref<Base::Object> source = GetItemsSource();
        Collections::IItemsSource* childSource = AsItemsSource(source.Get());
        if (childSource == nullptr) {
            childSource =
                static_cast<Collections::IItemsSource*>(
                    &ItemsControl::GetItems());
        }
        AeroGuiInternal::SetItemsSourceBorrowed(
            *childItems_, childSource);
    }
    static_cast<void>(SynchronizeTemplate());
    ProjectRealizedHeaders();
}

void TreeViewItem::OnTemplateDetached() noexcept {
    if (childItems_ != nullptr) {
        AeroGuiInternal::SetItemsSourceBorrowed(
            *childItems_, nullptr);
    }
    headerText_ = nullptr;
    iconText_ = nullptr;
    expanderGlyph_ = nullptr;
    childItems_ = nullptr;
    itemsBorder_ = nullptr;
    itemsHostPresenter_ = nullptr;
    if (expandButton_ != nullptr) {
        static_cast<void>(expandButton_->RemoveHandler(
            ButtonBase::ClickEvent, expandClickHandler_));
        expandButton_ = nullptr;
    }
    HeaderedItemsControl::OnTemplateDetached();
}

Base::Result<void>
TreeViewItem::SynchronizeTemplate() noexcept {
    if (headerText_ != nullptr) {
        const Value header = GetHeader();
        headerText_->SetText(
            header.Kind() == ValueKind::String
            ? header.AsString()
            : Base::StringView{});
    }
    if (iconText_ != nullptr) {
        iconText_->SetText(GetIcon());
    }
    if (expanderGlyph_ != nullptr) {
        expanderGlyph_->SetText(
                !GetHasItems()
                ? Base::StringView("")
                : GetIsExpanded()
                    ? Base::StringView("v")
                    : Base::StringView(">"));
    }
    if (childItems_ != nullptr) {
        childItems_->SetVisibility(
                GetIsExpanded()
                ? Visibility::Visible
                : Visibility::Collapsed);
    }
    const bool expanded = GetIsExpanded();
    if (itemsBorder_ != nullptr) {
        itemsBorder_->SetVisibility(
            expanded
                ? Visibility::Visible
                : Visibility::Collapsed);
    }
    if (itemsHostPresenter_ == nullptr && itemsBorder_ != nullptr) {
        if (auto* border = ::Aero::TryCast<Decorator>(itemsBorder_)) {
            itemsHostPresenter_ = border->GetChild();
        }
    }
    if (itemsHostPresenter_ != nullptr) {
        Base::Ref<Media::Transform> transform =
            itemsHostPresenter_->GetRenderTransform();
        Media::ScaleTransform* scale =
            transform
                ? ::Aero::TryCast<Media::ScaleTransform>(transform.Get())
                : nullptr;
        if (scale == nullptr) {
            Base::Result<Base::Ref<Media::ScaleTransform>> made =
                Base::MakeRef<Media::ScaleTransform>();
            if (made) {
                scale = made.Value().Get();
                itemsHostPresenter_->SetRenderTransform(
                    Base::Ref<Media::Transform>(made.Value()));
            }
        }
        if (scale != nullptr) {
            if (::Aero::AnimationEngine* animations =
                    AeroGuiInternal::AnimationEngineOf(*this)) {
                static_cast<void>(animations->RemoveTarget(*scale));
                static_cast<void>(
                    animations->RemoveTarget(*itemsHostPresenter_));
                if (itemsBorder_ != nullptr) {
                    static_cast<void>(
                        animations->RemoveTarget(*itemsBorder_));
                }
            }
            scale->SetScaleY(expanded ? 1.0 : 0.0);
        }
    }
    if (DependencyObject* arrow = GetTemplateChild("Arrow")) {
        if (auto* element = ::Aero::TryCast<UIElement>(arrow)) {
            Base::Ref<Media::Transform> transform =
                element->GetRenderTransform();
            if (auto* rotate =
                    transform
                        ? ::Aero::TryCast<Media::RotateTransform>(
                              transform.Get())
                        : nullptr) {
                rotate->SetAngle(expanded ? 90.0 : 0.0);
            }
        }
    }
    return {};
}

void TreeViewItem::OnHeaderChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
    noexcept {
    static_cast<void>(SynchronizeTemplate());
    ProjectHeaderContent();
}

namespace {

ContentPresenter* FindHeaderPresenter(
    TreeViewItem& item,
    UIElement* expandButton,
    DependencyObject* namedHeader) noexcept {
    if (auto* presenter =
            namedHeader != nullptr
                ? ::Aero::TryCast<ContentPresenter>(namedHeader)
                : nullptr) {
        return presenter;
    }

    ContentPresenter* headerPresenter = nullptr;
    const auto consider = [&](ContentPresenter* presenter) noexcept {
        if (presenter == nullptr) {
            return;
        }
        if (presenter->GetContentSource() == Base::StringView("Header")) {
            headerPresenter = presenter;
        }
    };
    const auto walk = [&](auto&& self, ::Aero::Media::Visual& node) -> void {
        if (headerPresenter != nullptr &&
            headerPresenter->GetContentSource() ==
                Base::StringView("Header")) {
            return;
        }
        if (&node != &item) {
            if (::Aero::TryCast<ItemsPresenter>(&node) != nullptr ||
                ::Aero::TryCast<TreeViewItem>(&node) != nullptr) {
                return;
            }
        }
        if (auto* presenter = ::Aero::TryCast<ContentPresenter>(&node)) {
            consider(presenter);
        }
        const std::uint32_t count =
            ::Aero::Media::VisualTreeHelper::GetChildrenCount(node);
        for (std::uint32_t index = 0U; index < count; ++index) {
            ::Aero::Media::Visual* child =
                ::Aero::Media::VisualTreeHelper::GetChild(node, index);
            if (child != nullptr) {
                self(self, *child);
            }
        }
    };
    walk(walk, item);

    if (headerPresenter != nullptr &&
        headerPresenter->GetContentSource() == Base::StringView("Header")) {
        return headerPresenter;
    }

    // PART_Header is authored as ToggleButton.Content. The expander style
    // template has not necessarily projected that content into the visual
    // tree yet, but the logical Content tree already holds the presenter.
    if (expandButton != nullptr) {
        const Value content =
            static_cast<Primitives::ToggleButton*>(expandButton)
                ->GetContent();
        if (content.Kind() == ValueKind::Object &&
            !content.IsNullObject() &&
            content.AsObject()) {
            Base::Object* hosted = content.AsObject().Get();
            if (item.PropertyRegistry().Types().IsDerivedFrom(
                    hosted->RuntimeType(),
                    ::Aero::Media::Visual::StaticTypeId())) {
                walk(walk, *static_cast<::Aero::Media::Visual*>(hosted));
            }
        }
    }
    return headerPresenter;
}

} // namespace

void TreeViewItem::ProjectHeaderContent() noexcept {
    const Value header = GetHeader();
    if (header.Kind() != ValueKind::Object ||
        header.IsNullObject() ||
        !header.AsObject()) {
        return;
    }
    Base::Object* obj = header.AsObject().Get();
    if (!PropertyRegistry().Types().IsDerivedFrom(
            obj->RuntimeType(), UIElement::StaticTypeId())) {
        return;
    }
    auto* element = static_cast<UIElement*>(obj);
    ContentPresenter* presenter = FindHeaderPresenter(
        *this, expandButton_, GetTemplateChild("PART_Header"));
    ElementTree* tree = GetTree();
    if (presenter == nullptr) {
        return;
    }
    if (tree == nullptr) {
        presenter->HostUiElement(header.AsObject(), *element);
        return;
    }
    HostHeaderVisual(
        *tree, *presenter, header.AsObject(), *element);
}

void TreeViewItem::ProjectRealizedHeaders() noexcept {
    ProjectHeaderContent();
    ItemContainerGenerator* generator = GetItemContainerGenerator();
    if (generator == nullptr) {
        return;
    }
    const std::uint32_t count = generator->GetGeneratedCount();
    for (std::uint32_t index = 0U; index < count; ++index) {
        FrameworkElement* container = generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !PropertyRegistry().Types().IsDerivedFrom(
                container->RuntimeType(), TreeViewItem::StaticTypeId())) {
            continue;
        }
        static_cast<TreeViewItem*>(container)->ProjectHeaderContent();
    }
}

void TreeViewItem::OnExpandedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    if (expanderGestureActive_ &&
        args.GetNewValue().AsBoolean() != expanderGestureTarget_) {
        SetIsExpanded(expanderGestureTarget_);
        return;
    }
    if (args.GetNewValue().AsBoolean()) {
        ActivateHierarchicalContent();
    }
    static_cast<void>(SynchronizeTemplate());
    RoutedEventArgs event;
    static_cast<void>(RaiseEvent(
        args.GetNewValue().AsBoolean()
            ? ExpandedEvent
            : CollapsedEvent,
        &event));
}

void TreeViewItem::OnSelectedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    RoutedEventArgs event;
    static_cast<void>(RaiseEvent(
        args.GetNewValue().AsBoolean()
            ? SelectedEvent
            : UnselectedEvent,
        &event));
}

void TreeViewItem::BeginExpanderGesture() noexcept {
    if (!GetHasItems()) return;
    expanderGestureTarget_ = !GetIsExpanded();
    expanderGestureActive_ = true;
    expanderGestureArmed_ = false;
}

void TreeViewItem::OnExpandButtonClick(
    Base::Object*,
    RoutedEventArgs&) noexcept {
    bool target;
    if (expanderGestureActive_) {
        target = expanderGestureTarget_;
        expanderGestureArmed_ = true;
    } else if (expandButton_ != nullptr) {
        const Nullable<bool> checked = expandButton_->GetIsChecked();
        target = checked.GetHasValue() && checked.GetValue();
    } else {
        return;
    }
    // Keep the gesture armed while writing so a TwoWay IsChecked echo
    // cannot collapse the node on the same click.
    if (expandButton_ != nullptr) {
        const Nullable<bool> checked = expandButton_->GetIsChecked();
        const bool isChecked =
            checked.GetHasValue() && checked.GetValue();
        if (isChecked != target) {
            expandButton_->SetIsChecked(Nullable<bool>{target});
        }
    }
    if (GetIsExpanded() != target) {
        SetIsExpanded(target);
    }
    static_cast<void>(SynchronizeTemplate());
    // Keep the gesture armed until the next expander MouseDown so a TwoWay
    // IsChecked echo cannot collapse the node on the same click.
}

void TreeViewItem::ApplyExpanderGesture() noexcept {
    if (!expanderGestureActive_) return;
    expanderGestureArmed_ = true;
    if (expandButton_ != nullptr) {
        expandButton_->SetIsChecked(Nullable<bool>{expanderGestureTarget_});
    }
    if (GetIsExpanded() != expanderGestureTarget_) {
        SetIsExpanded(expanderGestureTarget_);
    }
    static_cast<void>(SynchronizeTemplate());
}

TreeView::~TreeView() {
    auto* behaviors = static_cast<ControlBehavior*>(
        AeroGuiInternal::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->Detach(*this));
    }
}

Base::Ref<Base::Object>
TreeView::GetSelectedItem() const noexcept {
    return GetValueOr(
        SelectedItemProperty,
        Base::Ref<Base::Object>{});
}

Base::Result<Base::Ref<FrameworkElement>>
TreeView::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<TreeViewItem>> made =
        Base::MakeRef<TreeViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

bool TreeView::SelectItem(
    TreeViewItem* item) noexcept {
    auto* states = static_cast<Aero::VisualStateManager*>(
        AeroGuiInternal::VisualStateRuntime(*this));
    Base::Ref<Base::Object> next;
    if (item != nullptr) {
        next = DataItemFromContainer(*item);
    }
    Base::Ref<Base::Object> previous =
        GetSelectedItem();
    if (previous.Get() == next.Get() &&
        (item == nullptr || item->GetIsSelected())) {
        return false;
    }
    UnselectOtherTreeViewItems(*this, item, states);
    if (item != nullptr) {
        item->SetIsSelected(true);
        if (states != nullptr) {
            // Gallery TreeViewItem chrome uses property Triggers, not VSM
            // SelectionStates. Missing visual states must not block
            // SelectedItem / SelectedItemChanged (Tag → SelectedSample).
            static_cast<void>(
                Aero::VisualStateManagerRuntime::GoToState(*states,
                    *item,
                    "SelectionStates",
                    "Selected"));
        }
    }
    SetReadOnlyCurrentValue(SelectedItemProperty, next);
    RoutedEventArgs event;
    RaiseEvent(SelectedItemChangedEvent, &event);
    if (Aero::BindingEngine* bindings =
            AeroGuiInternal::BindingEngineOf(*this)) {
        static_cast<void>(bindings->Flush());
    }
    if (item != nullptr) {
        ::Aero::Media::Visual* visual = item->GetVisualParent();
        while (visual != nullptr) {
            if (auto* node = ::Aero::TryCast<TreeViewItem>(visual)) {
                if (!node->GetIsExpanded()) {
                    node->SetIsExpanded(true);
                }
                static_cast<void>(node->SynchronizeTemplate());
            }
            visual = visual->GetVisualParent();
        }
        static_cast<void>(item->SynchronizeTemplate());
    }
    return true;
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

TreeBehavior::
TreeBehavior(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input,
    VisualStateManager* states) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      states_(states),
      mouseDownHandler_(
          this,
          &TreeBehavior::
              OnMouseDown),
      keyDownHandler_(
          this,
          &TreeBehavior::
              OnKeyDown) {}

TreeBehavior::
~TreeBehavior() noexcept {
    while (!records_.Empty()) {
        TreeView* treeView =
            ResolveTreeView(
                records_.Size() - 1U);
        if (treeView == nullptr) {
            records_.PopBack();
        } else {
            static_cast<void>(
                Detach(*treeView));
        }
    }
}

std::uint32_t
TreeBehavior::FindTreeView(
    const TreeView& treeView) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index]) ==
            &treeView) {
            return index;
        }
    }
    return UINT32_MAX;
}

TreeView*
TreeBehavior::ResolveTreeView(
    std::uint32_t index) noexcept {
    ::Aero::Media::Visual* visual =
        index < records_.Size()
        ? tree_->ResolveHandle(records_[index])
        : nullptr;
    return visual != nullptr
        ? static_cast<TreeView*>(
            ::Aero::TryCast<::Aero::UIElement>(visual))
        : nullptr;
}

Base::Result<void>
TreeBehavior::Attach(
    TreeView& treeView) noexcept {
    if (treeView.GetTree() != tree_ ||
        FindTreeView(treeView) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TreeView interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(treeView);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        treeView.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_,
            true);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        treeView.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    if (!key) {
        static_cast<void>(
            treeView.RemoveHandler(
                UIElement::MouseDownEvent,
                mouseDownHandler_));
        return key.GetStatus();
    }
    Base::Result<void> stored =
        records_.PushBack(handle.Value());
    if (!stored) {
        static_cast<void>(
            treeView.RemoveHandler(
                UIElement::KeyDownEvent,
                keyDownHandler_));
        static_cast<void>(
            treeView.RemoveHandler(
                UIElement::MouseDownEvent,
                mouseDownHandler_));
        return stored.GetStatus();
    }
    return {};
}

Base::Result<bool>
TreeBehavior::Detach(
    TreeView& treeView) noexcept {
    const std::uint32_t index =
        FindTreeView(treeView);
    if (index == UINT32_MAX) return false;
    static_cast<void>(treeView.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(treeView.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    for (std::uint32_t current = index;
        current + 1U < records_.Size();
        ++current) {
        records_[current] =
            records_[current + 1U];
    }
    records_.PopBack();
    return true;
}

TreeViewItem*
TreeBehavior::FindItem(
    TreeView& treeView,
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !treeView.PropertyRegistry().Types().
            IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return nullptr;
    }
    ::Aero::Media::Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != &treeView) {
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            treeView.PropertyRegistry().Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    TreeViewItem::StaticTypeId())) {
            return static_cast<TreeViewItem*>(
                element);
        }
        visual = visual->GetVisualParent();
    }
    return nullptr;
}

Base::Result<void>
TreeBehavior::CollectVisibleItems(
    ::Aero::Media::Visual& parent,
    Base::Vector<TreeViewItem*>& items)
    noexcept {
    for (::Aero::Media::Visual* child :
        AeroGuiInternal::RenderChildren(parent)) {
        if (child == nullptr) continue;
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(child);
        if (element != nullptr &&
            element->GetVisibility() !=
                Visibility::Visible) {
            continue;
        }
        if (element != nullptr &&
            element->PropertyRegistry().Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    TreeViewItem::StaticTypeId())) {
            Base::Result<void> added =
                items.PushBack(
                    static_cast<TreeViewItem*>(
                        element));
            if (!added) return added.GetStatus();
        }
        Base::Result<void> nested =
            CollectVisibleItems(*child, items);
        if (!nested) return nested.GetStatus();
    }
    return {};
}

void TreeBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args)
    noexcept {
    if (args.GetChangedButton() !=
        MouseButton::Left) {
        return;
    }
    auto& treeView =
        *static_cast<TreeView*>(sender);
    if (!treeView.GetIsEnabled()) return;
    TreeViewItem* item =
        FindItem(
            treeView, args.GetOriginalSource());
    if (item == nullptr) return;
    UIElement* sourceElement =
        args.GetOriginalSource() != nullptr &&
                treeView.PropertyRegistry().Types().IsDerivedFrom(
                    args.GetOriginalSource()->RuntimeType(),
                    UIElement::StaticTypeId())
            ? static_cast<UIElement*>(args.GetOriginalSource())
            : nullptr;
    const auto inItemsRegion =
        [](TreeViewItem& node, UIElement* source) noexcept -> bool {
        if (source == nullptr) return false;
        Controls::Panel* host = node.GetItemsHost();
        ::Aero::Media::Visual* visual = source;
        while (visual != nullptr && visual != &node) {
            if (visual == host) return true;
            if (::Aero::TryCast<ItemsPresenter>(visual) != nullptr) {
                return true;
            }
            visual = visual->GetVisualParent();
        }
        return false;
    };
    if (inItemsRegion(*item, sourceElement)) {
        // Gallery leaves put PART_Header inside a HasItems=False disabled
        // ExpandButton. Hits often land on the parent's ItemsPresenter
        // instead of the leaf. Pick the child whose box contains the point
        // so sibling rows stay expanded and SelectedItem is the Sample.
        TreeViewItem* child = nullptr;
        ::Aero::Media::Visual* walk = sourceElement;
        while (walk != nullptr && walk != item) {
            if (auto* node = ::Aero::TryCast<TreeViewItem>(walk)) {
                child = node;
                break;
            }
            walk = walk->GetVisualParent();
        }
        if (child == nullptr && sourceElement != nullptr) {
            const Base::Point screen =
                sourceElement->PointToScreen(args.GetPosition());
            ItemContainerGenerator* generator =
                item->GetItemContainerGenerator();
            const std::uint32_t count = item->GetCount();
            for (std::uint32_t index = 0U; index < count; ++index) {
                FrameworkElement* container =
                    generator != nullptr
                    ? generator->ContainerFromIndex(index)
                    : nullptr;
                auto* candidate =
                    container != nullptr
                    ? ::Aero::TryCast<TreeViewItem>(container)
                    : nullptr;
                if (candidate == nullptr) continue;
                Base::Point local;
                if (!candidate->TryPointFromScreen(screen, local)) {
                    continue;
                }
                Size size = candidate->GetRenderSize();
                if (!(size.width > 0.0 && size.height > 0.0)) {
                    const Rect slot = candidate->GetLayoutSlot();
                    size = Size{slot.width, slot.height};
                }
                if (local.x >= 0.0 && local.y >= 0.0 &&
                    local.x < size.width && local.y < size.height) {
                    child = candidate;
                    break;
                }
            }
        }
        if (child != nullptr) {
            item = child;
        }
    }
    // Expand/collapse is owned by ExpandButton Click + TwoWay IsChecked.
    // MouseDown must not toggle: gallery leaves live inside a disabled
    // ExpandButton, so a miss lands on the parent Grid and would collapse
    // Basic Input (hiding RepeatButton/ToggleButton/CheckBox/RadioButton/Slider).
    static_cast<void>(treeView.SelectItem(item));
    static_cast<void>(
        input_->SetFocus(item));
    args.SetHandled(true);
}

void TreeBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown &&
        args.GetKey() != KeyboardKeyLeft &&
        args.GetKey() != KeyboardKeyRight &&
        args.GetKey() != KeyboardKeyEnter &&
        args.GetKey() != KeyboardKeySpace) {
        return;
    }
    auto& treeView =
        *static_cast<TreeView*>(sender);
    TreeViewItem* current =
        FindItem(
            treeView, args.GetOriginalSource());
    if (current == nullptr) {
        Base::Ref<Base::Object> selected =
            treeView.GetSelectedItem();
        if (selected &&
            treeView.PropertyRegistry().Types().
                IsDerivedFrom(
                    selected->RuntimeType(),
                    TreeViewItem::StaticTypeId())) {
            current =
                static_cast<TreeViewItem*>(
                    selected.Get());
        } else if (selected) {
            Base::Vector<TreeViewItem*> visible;
            if (CollectVisibleItems(treeView, visible)) {
                for (TreeViewItem* candidate : visible) {
                    if (candidate != nullptr &&
                        candidate->GetIsSelected()) {
                        current = candidate;
                        break;
                    }
                }
            }
        }
    }
    if (current == nullptr) return;
    if (args.GetKey() == KeyboardKeyRight &&
        AeroGuiInternal::TreeViewItemCount(*current) != 0U &&
        !current->GetIsExpanded()) {
        static_cast<void>(
            current->SetIsExpanded(true));
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyLeft &&
        current->GetIsExpanded()) {
        static_cast<void>(
            current->SetIsExpanded(false));
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyEnter ||
        args.GetKey() == KeyboardKeySpace) {
        if (AeroGuiInternal::TreeViewItemCount(*current) != 0U) {
            static_cast<void>(
                current->SetIsExpanded(
                    !current->GetIsExpanded()));
        }
        treeView.SelectItem(current);
        args.SetHandled(true);
        return;
    }
    Base::Vector<TreeViewItem*> visible;
    Base::Result<void> collected =
        CollectVisibleItems(
            treeView, visible);
    if (!collected || visible.Empty()) return;
    std::uint32_t index = UINT32_MAX;
    for (std::uint32_t currentIndex = 0U;
        currentIndex < visible.Size();
        ++currentIndex) {
        if (visible[currentIndex] == current) {
            index = currentIndex;
            break;
        }
    }
    if (index == UINT32_MAX) return;
    std::uint32_t target = index;
    if (args.GetKey() == KeyboardKeyUp &&
        target > 0U) {
        --target;
    } else if (
        args.GetKey() == KeyboardKeyDown &&
        target + 1U < visible.Size()) {
        ++target;
    } else {
        return;
    }
    treeView.SelectItem(visible[target]);
    static_cast<void>(
        input_->SetFocus(visible[target]));
    args.SetHandled(true);
}

} // namespace Aero::Controls

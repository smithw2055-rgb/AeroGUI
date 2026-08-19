#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ControlTemplate.hpp>

#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::TreeBehavior;

using namespace Primitives;

namespace {

// RTTI is disabled, so an ItemsSource arrives as a Base::Object and must be
// down-cast by its runtime type. The collection contract is implemented by the
// two observable collections known to the framework (mirrors
// metadata/Support.inl OnItemsSourceChanged).
Collections::IItemsSource* AsItemsSource(
    Base::Object* source) noexcept {
    if (source == nullptr) return nullptr;
    const Meta::TypeId type = source->RuntimeType();
    if (type == Collections::ObservableCollection::StaticTypeId()) {
        return static_cast<Collections::ObservableCollection*>(source);
    }
    if (type == Media::GradientStopCollection::StaticTypeId()) {
        return static_cast<Media::GradientStopCollection*>(source);
    }
    return nullptr;
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
          this, &TreeViewItem::OnSelectedChanged) {
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
        ItemsControl::Access::SetItemsSourceBorrowed(
            *childItems_, nullptr);
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
        auto* bindings = ::Aero::Media::Visual::Access::BindingEngineFor(
            *this);
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
            Meta::UpdateSourceTrigger::PropertyChanged;
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
        ItemsControl::Access::SetItemsSourceBorrowed(
            *childItems_, items);
    }
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
    if (headerText_ == nullptr ||
        expanderGlyph_ == nullptr ||
        childItems_ == nullptr) {
        return;
    }
Base::Ref<Base::Object> source = GetItemsSource();
    Collections::IItemsSource* childSource = AsItemsSource(source.Get());
    if (childSource == nullptr) {
        childSource =
            static_cast<Collections::IItemsSource*>(&ItemsControl::GetItems());
    }
    ItemsControl::Access::SetItemsSourceBorrowed(
        *childItems_, childSource);
    static_cast<void>(SynchronizeTemplate());
}

void TreeViewItem::OnTemplateDetached() noexcept {
    if (childItems_ != nullptr) {
        ItemsControl::Access::SetItemsSourceBorrowed(
            *childItems_, nullptr);
    }
    headerText_ = nullptr;
    iconText_ = nullptr;
    expanderGlyph_ = nullptr;
    childItems_ = nullptr;
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
    return {};
}

void TreeViewItem::OnHeaderChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
    noexcept {
    static_cast<void>(SynchronizeTemplate());
}

void TreeViewItem::OnExpandedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
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

TreeView::~TreeView() {
    auto* behaviors = static_cast<ControlBehavior*>(
        ::Aero::Media::Visual::Access::ControlBehaviorRuntime(*this));
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
        ::Aero::Media::Visual::Access::VisualStateRuntime(*this));
    Base::Ref<Base::Object> previous =
        GetSelectedItem();
    if (previous.Get() == item) return false;
    if (previous &&
        PropertyRegistry().Types().IsDerivedFrom(
            previous->RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        static_cast<TreeViewItem*>(previous.Get())->SetIsSelected(false);
        if (states != nullptr) {
            static_cast<void>(
                Aero::Controls::TemplatePrivate::GoToState(*states,
                    *static_cast<TreeViewItem*>(
                        previous.Get()),
                    "SelectionStates",
                    "Unselected"));
        }
    }
    Base::Ref<Base::Object> next;
    if (item != nullptr) {
        item->SetIsSelected(true);
        if (states != nullptr) {
            Base::Result<bool> state =
                Aero::Controls::TemplatePrivate::GoToState(*states,
                    *item,
                    "SelectionStates",
                    "Selected");
            if (!state) {
                return false;
            }
        }
        next =
            Base::Ref<Base::Object>::FromBorrowed(
                *item);
    }
    SetReadOnlyCurrentValue(SelectedItemProperty, next);
    RoutedEventArgs event;
    RaiseEvent(SelectedItemChangedEvent, &event);
    return true;
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

TreeView::Access::
Access(
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
          &TreeView::Access::
              OnMouseDown),
      keyDownHandler_(
          this,
          &TreeView::Access::
              OnKeyDown) {}

TreeView::Access::
~Access() noexcept {
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
TreeView::Access::FindTreeView(
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
TreeView::Access::ResolveTreeView(
    std::uint32_t index) noexcept {
    ::Aero::Media::Visual* visual =
        index < records_.Size()
        ? tree_->ResolveHandle(records_[index])
        : nullptr;
    return visual != nullptr
        ? static_cast<TreeView*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void>
TreeView::Access::Attach(
    TreeView& treeView) noexcept {
    if (Aero::ElementPrivate::Tree(treeView) != tree_ ||
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
            mouseDownHandler_);
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
TreeView::Access::Detach(
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
TreeView::Access::FindItem(
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
            visual->AsUIElement();
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
TreeView::Access::CollectVisibleItems(
    ::Aero::Media::Visual& parent,
    Base::Vector<TreeViewItem*>& items)
    noexcept {
    for (::Aero::Media::Visual* child :
        Aero::ElementPrivate::VisualChildren(parent)) {
        if (child == nullptr) continue;
        UIElement* element =
            child->AsUIElement();
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

void TreeView::Access::OnMouseDown(
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
    if (!treeView.SelectItem(item)) return;
    static_cast<void>(
        input_->SetFocus(item));
    args.SetHandled(true);
}

void TreeView::Access::OnKeyDown(
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
        }
    }
    if (current == nullptr) return;
    if (args.GetKey() == KeyboardKeyRight &&
        ::Aero::Media::Visual::Access::TreeViewItemCount(*current) != 0U &&
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
        if (::Aero::Media::Visual::Access::TreeViewItemCount(*current) != 0U) {
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

#include "TemplateInternals.hpp"
#include <Aero/Controls/Items.hpp>
#include <Aero/Styling.hpp>

#include <utility>
#include "gui/RoutedEventInternal.hpp"
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Detail::TreeBehavior;

using namespace Primitives;

TreeViewItem::TreeViewItem() noexcept
    : TreeViewItem(StaticTypeId()) {}

TreeViewItem::TreeViewItem(
    TypeId runtimeType) noexcept
    : ItemContainer(runtimeType),
      headerChangedHandler_(
          this, &TreeViewItem::OnHeaderChanged),
      iconChangedHandler_(
          this, &TreeViewItem::OnHeaderChanged),
      expandedChangedHandler_(
          this, &TreeViewItem::OnExpandedChanged),
      selectedChangedHandler_(
          this, &TreeViewItem::OnSelectedChanged),
      itemsChangedHandler_(
          this, &TreeViewItem::OnItemsChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        HeaderProperty, headerChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IconProperty, iconChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsExpandedProperty,
        expandedChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsSelectedProperty,
        selectedChangedHandler_));
    static_cast<void>(items_.TryAddItemsChanged(
        itemsChangedHandler_));
}

TreeViewItem::~TreeViewItem() {
    if (childItems_ != nullptr) {
        static_cast<void>(
            childItems_->SetItemsSource(nullptr));
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
    static_cast<void>(items_.RemoveItemsChanged(
        itemsChangedHandler_));
}

Base::StringView
TreeViewItem::Header() const noexcept {
    return GetValueOr(
        HeaderProperty, Base::StringView{});
}

Base::Result<void> TreeViewItem::SetHeader(
    Base::StringView value) noexcept {
    return SetValue(HeaderProperty, value);
}

Base::StringView TreeViewItem::Icon() const noexcept {
    return GetValueOr(
        IconProperty, Base::StringView{});
}

Base::Result<void> TreeViewItem::SetIcon(
    Base::StringView value) noexcept {
    return SetValue(IconProperty, value);
}

Base::Ref<DataTemplate>
TreeViewItem::HeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

Base::Result<void>
TreeViewItem::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    return SetValue(
        HeaderTemplateProperty, std::move(value));
}

bool TreeViewItem::IsExpanded() const noexcept {
    return GetValueOr(
        IsExpandedProperty, false);
}

Base::Result<void>
TreeViewItem::SetIsExpanded(
    bool value) noexcept {
    return SetCurrentValue(
        IsExpandedProperty, value);
}

bool TreeViewItem::IsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

Base::Result<void>
TreeViewItem::SetIsSelected(
    bool value) noexcept {
    return SetCurrentValue(
        IsSelectedProperty, value);
}

void TreeViewItem::OnItemsChanged(
    const ItemsChangedEvent&) noexcept {
    static_cast<void>(SetReadOnlyCurrentValue(
        HasItemsProperty, Count() != 0U));
}

Base::Result<void>
TreeViewItem::OnApplyTemplate() noexcept {
    Base::Result<void> applied =
        ItemContainer::OnApplyTemplate();
    if (!applied) return applied.GetStatus();

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
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TreeViewItem template requires HeaderText, ExpanderGlyph, and ChildItems parts");
    }
    Base::Result<void> source =
        childItems_->SetItemsSource(this);
    if (!source) return source.GetStatus();
    return SynchronizeTemplate();
}

void TreeViewItem::OnTemplateDetached() noexcept {
    if (childItems_ != nullptr) {
        static_cast<void>(
            childItems_->SetItemsSource(nullptr));
    }
    headerText_ = nullptr;
    iconText_ = nullptr;
    expanderGlyph_ = nullptr;
    childItems_ = nullptr;
    ItemContainer::OnTemplateDetached();
}

Base::Result<void>
TreeViewItem::SynchronizeTemplate() noexcept {
    if (headerText_ != nullptr) {
        Base::Result<void> header =
            headerText_->SetText(Header());
        if (!header) return header.GetStatus();
    }
    if (iconText_ != nullptr) {
        Base::Result<void> icon =
            iconText_->SetText(Icon());
        if (!icon) return icon.GetStatus();
    }
    if (expanderGlyph_ != nullptr) {
        Base::Result<void> glyph =
            expanderGlyph_->SetText(
                Count() == 0U
                ? Base::StringView("")
                : IsExpanded()
                    ? Base::StringView("v")
                    : Base::StringView(">"));
        if (!glyph) return glyph.GetStatus();
    }
    if (childItems_ != nullptr) {
        Base::Result<void> visible =
            childItems_->SetVisibility(
                IsExpanded()
                ? Visibility::Visible
                : Visibility::Collapsed);
        if (!visible) return visible.GetStatus();
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
    if (interactions_ != nullptr) {
        static_cast<void>(
            static_cast<TreeBehavior*>(
                interactions_)->Detach(*this));
    }
}

Base::Ref<Base::Object>
TreeView::SelectedItem() const noexcept {
    return GetValueOr(
        SelectedItemProperty,
        Base::Ref<Base::Object>{});
}

Base::Result<Base::Ref<ItemContainer>>
TreeView::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<TreeViewItem>> made =
        Base::MakeRef<TreeViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

Base::Result<bool> TreeView::SelectItem(
    TreeViewItem* item) noexcept {
    Base::Ref<Base::Object> previous =
        SelectedItem();
    if (previous.Get() == item) return false;
    if (previous &&
        PropertyRegistry().Types().IsDerivedFrom(
            previous->RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        Base::Result<void> cleared =
            static_cast<TreeViewItem*>(
                previous.Get())->SetIsSelected(
                    false);
        if (!cleared) return cleared.GetStatus();
        if (states_ != nullptr) {
            static_cast<void>(
                Aero::Controls::Detail::TemplatePrivate::GoToState(*states_,
                    *static_cast<TreeViewItem*>(
                        previous.Get()),
                    "SelectionStates",
                    "Unselected"));
        }
    }
    Base::Ref<Base::Object> next;
    if (item != nullptr) {
        Base::Result<void> selected =
            item->SetIsSelected(true);
        if (!selected) {
            if (previous) {
                static_cast<void>(
                    static_cast<TreeViewItem*>(
                        previous.Get())->
                        SetIsSelected(true));
            }
            return selected.GetStatus();
        }
        if (states_ != nullptr) {
            Base::Result<bool> state =
                Aero::Controls::Detail::TemplatePrivate::GoToState(*states_,
                    *item,
                    "SelectionStates",
                    "Selected");
            if (!state) {
                return state.GetStatus();
            }
        }
        next =
            Base::Ref<Base::Object>::FromBorrowed(
                *item);
    }
    Base::Result<void> published =
        SetReadOnlyCurrentValue(
            SelectedItemProperty, next);
    if (!published) return published.GetStatus();
    RoutedEventArgs event;
    Base::Result<void> raised =
        RaiseEvent(
            SelectedItemChangedEvent, &event);
    if (!raised) return raised.GetStatus();
    return true;
}

} // namespace Aero::Controls

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Controls;

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
    Visual* visual =
        index < records_.Size()
        ? tree_->ResolveHandle(records_[index])
        : nullptr;
    return visual != nullptr
        ? static_cast<TreeView*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void>
TreeBehavior::Attach(
    TreeView& treeView) noexcept {
    if (treeView.interactions_ != nullptr ||
        Aero::Detail::ElementPrivate::Tree(treeView) != tree_ ||
        FindTreeView(treeView) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TreeView interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(treeView);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        treeView.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        treeView.TryAddHandler(
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
        records_.TryPushBack(handle.Value());
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
    treeView.interactions_ = this;
    treeView.states_ = states_;
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
    treeView.interactions_ = nullptr;
    treeView.states_ = nullptr;
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
    Visual* visual =
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
TreeBehavior::CollectVisibleItems(
    Visual& parent,
    Base::Vector<TreeViewItem*>& items)
    noexcept {
    for (Visual* child :
        Aero::Detail::ElementPrivate::VisualChildren(parent)) {
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
                items.TryPushBack(
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
    Base::Result<bool> selected =
        treeView.SelectItem(item);
    if (!selected &&
        selected.GetStatus().code !=
            Base::ErrorCode::Ok) {
        return;
    }
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
            treeView.SelectedItem();
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
        current->Count() != 0U &&
        !current->IsExpanded()) {
        static_cast<void>(
            current->SetIsExpanded(true));
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyLeft &&
        current->IsExpanded()) {
        static_cast<void>(
            current->SetIsExpanded(false));
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyEnter ||
        args.GetKey() == KeyboardKeySpace) {
        if (current->Count() != 0U) {
            static_cast<void>(
                current->SetIsExpanded(
                    !current->IsExpanded()));
        }
        static_cast<void>(
            treeView.SelectItem(current));
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
    static_cast<void>(
        treeView.SelectItem(visible[target]));
    static_cast<void>(
        input_->SetFocus(visible[target]));
    args.SetHandled(true);
}

} // namespace Aero::Detail

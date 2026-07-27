#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Templates.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>

#include <algorithm>
#include <utility>

namespace Aero::Controls {
namespace {

bool ContainsIndex(
    Base::Span<const std::uint32_t> values,
    std::uint32_t index) noexcept {
    for (std::uint32_t value : values) {
        if (value == index) return true;
    }
    return false;
}

Base::Result<void> InsertSortedUnique(
    Base::Vector<std::uint32_t>& values,
    std::uint32_t value) noexcept {
    std::uint32_t index = 0U;
    while (index < values.Size() &&
        values[index] < value) {
        ++index;
    }
    if (index < values.Size() &&
        values[index] == value) {
        return {};
    }
    Base::Result<void> reserved =
        values.TryReserve(values.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> appended =
        values.TryPushBack(value);
    if (!appended) return appended.GetStatus();
    for (std::uint32_t current =
            values.Size() - 1U;
        current > index; --current) {
        values[current] = values[current - 1U];
    }
    values[index] = value;
    return {};
}

bool EqualIndices(
    Base::Span<const std::uint32_t> first,
    Base::Span<const std::uint32_t> second) noexcept {
    if (first.Size() != second.Size()) return false;
    for (std::uint32_t index = 0U;
        index < first.Size(); ++index) {
        if (first[index] != second[index]) return false;
    }
    return true;
}

} // namespace

bool ListBoxItem::IsSelected() const noexcept {
    return GetValueOr(IsSelectedProperty, false);
}

Base::Result<void> ListBoxItem::SetIsSelected(
    bool value) noexcept {
    return SetCurrentValue(IsSelectedProperty, value);
}

Selector::Selector() noexcept
    : Selector(StaticTypeId()) {}

Selector::Selector(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType),
      itemsChangedHandler_(
          this, &Selector::OnItemsChanged),
      propertyChangedHandler_(
          this, &Selector::OnPropertyChanged) {
    static_cast<void>(
        TryAddItemsChanged(itemsChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        SelectionModeProperty,
        propertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        SelectedIndexProperty,
        propertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        SelectedItemProperty,
        propertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        SelectedValueProperty,
        propertyChangedHandler_));
}

Selector::~Selector() {
    static_cast<void>(
        RemoveItemsChanged(itemsChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        SelectionModeProperty,
        propertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        SelectedIndexProperty,
        propertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        SelectedItemProperty,
        propertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        SelectedValueProperty,
        propertyChangedHandler_));
}

SelectionMode Selector::GetSelectionMode() const noexcept {
    return GetValueOr(
        SelectionModeProperty, SelectionMode::Single);
}

std::uint32_t Selector::SelectedIndex() const noexcept {
    return GetValueOr(SelectedIndexProperty, UINT32_MAX);
}

Base::Ref<Base::Object>
Selector::SelectedItem() const noexcept {
    return GetValueOr(
        SelectedItemProperty,
        Base::Ref<Base::Object>{});
}

Base::Ref<Base::Object>
Selector::SelectedValue() const noexcept {
    return GetValueOr(
        SelectedValueProperty,
        Base::Ref<Base::Object>{});
}

bool Selector::IsSelected(
    std::uint32_t index) const noexcept {
    return ContainsIndex(
        {selectedIndices_.Data(),
         selectedIndices_.Size()},
        index);
}

std::uint32_t Selector::IndexOfItem(
    const Base::Object* item) const noexcept {
    if (item == nullptr) return UINT32_MAX;
    for (std::uint32_t index = 0U;
        index < ItemCount(); ++index) {
        if (ItemAt(index).Get() == item) return index;
    }
    return UINT32_MAX;
}

Base::Result<void> Selector::SetSelectionMode(
    SelectionMode value) noexcept {
    lastSelectionError_ = {};
    Base::Result<void> stored =
        SetValue(SelectionModeProperty, value);
    if (!stored) return stored.GetStatus();
    if (value == SelectionMode::Single &&
        selectedIndices_.Size() > 1U) {
        const std::uint32_t selected =
            primaryIndex_ != UINT32_MAX
            ? primaryIndex_
            : selectedIndices_[0U];
        const std::uint32_t values[] = {
            selected};
        Base::Result<bool> normalized =
            ApplySelection(values, selected);
        if (!normalized) {
            return normalized.GetStatus();
        }
    }
    return lastSelectionError_.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(lastSelectionError_);
}

Base::Result<bool> Selector::SetSelectedIndex(
    std::uint32_t index) noexcept {
    if (index != UINT32_MAX &&
        index >= ItemCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector selected index is out of range");
    }
    if (index == UINT32_MAX) {
        return ClearSelection();
    }
    const std::uint32_t values[] = {index};
    return ApplySelection(values, index);
}

Base::Result<bool> Selector::SetSelectedItem(
    Base::Ref<Base::Object> item) noexcept {
    if (!item) return ClearSelection();
    const std::uint32_t index =
        IndexOfItem(item.Get());
    if (index == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Selector selected item is not in Items");
    }
    return SetSelectedIndex(index);
}

Base::Result<bool> Selector::SetSelectedValue(
    Base::Ref<Base::Object> value) noexcept {
    return SetSelectedItem(std::move(value));
}

Base::Result<bool> Selector::Select(
    std::uint32_t index) noexcept {
    if (index >= ItemCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector select index is out of range");
    }
    if (GetSelectionMode() == SelectionMode::Single) {
        return SetSelectedIndex(index);
    }
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.TryReserve(
            selectedIndices_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t selected :
        selectedIndices_) {
        Base::Result<void> copied =
            selection.TryPushBack(selected);
        if (!copied) return copied.GetStatus();
    }
    Base::Result<void> inserted =
        InsertSortedUnique(selection, index);
    if (!inserted) return inserted.GetStatus();
    return ApplySelection(
        {selection.Data(), selection.Size()},
        index);
}

Base::Result<bool> Selector::Unselect(
    std::uint32_t index) noexcept {
    if (!IsSelected(index)) return false;
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.TryReserve(
            selectedIndices_.Size() - 1U);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t selected :
        selectedIndices_) {
        if (selected == index) continue;
        Base::Result<void> copied =
            selection.TryPushBack(selected);
        if (!copied) return copied.GetStatus();
    }
    const std::uint32_t primary =
        primaryIndex_ != index
        ? primaryIndex_
        : (selection.Empty()
            ? UINT32_MAX
            : selection.Back());
    return ApplySelection(
        {selection.Data(), selection.Size()},
        primary);
}

Base::Result<bool> Selector::Toggle(
    std::uint32_t index) noexcept {
    return IsSelected(index)
        ? Unselect(index)
        : Select(index);
}

Base::Result<bool> Selector::SelectRange(
    std::uint32_t first,
    std::uint32_t last,
    bool preserveExisting) noexcept {
    if (first >= ItemCount() ||
        last >= ItemCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector range is out of range");
    }
    if (GetSelectionMode() == SelectionMode::Single) {
        return SetSelectedIndex(last);
    }
    const std::uint32_t begin =
        std::min(first, last);
    const std::uint32_t end =
        std::max(first, last);
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.TryReserve(
            (preserveExisting
                ? selectedIndices_.Size()
                : 0U) +
            (end - begin + 1U));
    if (!reserved) return reserved.GetStatus();
    if (preserveExisting) {
        for (std::uint32_t selected :
            selectedIndices_) {
            Base::Result<void> copied =
                selection.TryPushBack(selected);
            if (!copied) return copied.GetStatus();
        }
    }
    for (std::uint32_t index = begin;
        index <= end; ++index) {
        Base::Result<void> inserted =
            InsertSortedUnique(selection, index);
        if (!inserted) return inserted.GetStatus();
    }
    return ApplySelection(
        {selection.Data(), selection.Size()},
        last);
}

Base::Result<bool> Selector::ClearSelection() noexcept {
    return ApplySelection({}, UINT32_MAX);
}

Base::Result<bool> Selector::ApplySelection(
    Base::Span<const std::uint32_t> indices,
    std::uint32_t primaryIndex) noexcept {
    Base::Vector<std::uint32_t> normalized;
    Base::Result<void> reserved =
        normalized.TryReserve(indices.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index : indices) {
        if (index >= ItemCount()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Selector selection contains an invalid index");
        }
        Base::Result<void> inserted =
            InsertSortedUnique(normalized, index);
        if (!inserted) return inserted.GetStatus();
    }
    if (GetSelectionMode() == SelectionMode::Single &&
        normalized.Size() > 1U) {
        const std::uint32_t selected =
            ContainsIndex(
                {normalized.Data(), normalized.Size()},
                primaryIndex)
            ? primaryIndex
            : normalized[0U];
        normalized.Clear();
        Base::Result<void> added =
            normalized.TryPushBack(selected);
        if (!added) return added.GetStatus();
    }
    if (normalized.Empty()) {
        primaryIndex = UINT32_MAX;
    } else if (!ContainsIndex(
            {normalized.Data(), normalized.Size()},
            primaryIndex)) {
        primaryIndex = normalized.Back();
    }
    const Base::Span<const std::uint32_t> oldSelection(
        selectedIndices_.Data(),
        selectedIndices_.Size());
    const Base::Span<const std::uint32_t> newSelection(
        normalized.Data(),
        normalized.Size());
    if (pendingIndex_ == UINT32_MAX &&
        primaryIndex_ == primaryIndex &&
        EqualIndices(oldSelection, newSelection)) {
        return false;
    }

    Base::Vector<std::uint32_t> removed;
    Base::Vector<std::uint32_t> added;
    Base::Result<void> removedReserve =
        removed.TryReserve(selectedIndices_.Size());
    if (!removedReserve) {
        return removedReserve.GetStatus();
    }
    Base::Result<void> addedReserve =
        added.TryReserve(normalized.Size());
    if (!addedReserve) return addedReserve.GetStatus();
    for (std::uint32_t index : selectedIndices_) {
        if (!ContainsIndex(newSelection, index)) {
            Base::Result<void> stored =
                removed.TryPushBack(index);
            if (!stored) return stored.GetStatus();
        }
    }
    for (std::uint32_t index : normalized) {
        if (!ContainsIndex(oldSelection, index)) {
            Base::Result<void> stored =
                added.TryPushBack(index);
            if (!stored) return stored.GetStatus();
        }
    }

    const std::uint32_t oldPrimary =
        primaryIndex_;
    Base::Ref<Base::Object> oldPrimaryItem =
        oldPrimary < ItemCount()
        ? ItemAt(oldPrimary)
        : Base::Ref<Base::Object>();
    selectedIndices_ = std::move(normalized);
    primaryIndex_ = primaryIndex;
    pendingIndex_ = UINT32_MAX;
    Base::Result<void> published =
        PublishProperties();
    if (!published) {
        lastSelectionError_ =
            published.GetStatus();
        return published.GetStatus();
    }
    SyncContainers();
    if (!selectionChanged_.Empty()) {
        SelectionChangedEvent event;
        event.removedIndices = {
            removed.Data(), removed.Size()};
        event.addedIndices = {
            added.Data(), added.Size()};
        event.oldPrimaryIndex = oldPrimary;
        event.newPrimaryIndex = primaryIndex_;
        event.oldPrimaryItem =
            std::move(oldPrimaryItem);
        event.newPrimaryItem =
            primaryIndex_ < ItemCount()
            ? ItemAt(primaryIndex_)
            : Base::Ref<Base::Object>();
        selectionChanged_.Invoke(*this, event);
    }
    lastSelectionError_ = {};
    return true;
}

Base::Result<void> Selector::PublishProperties() noexcept {
    synchronizingProperties_ = true;
    const Base::Ref<Base::Object> selected =
        primaryIndex_ < ItemCount()
        ? ItemAt(primaryIndex_)
        : Base::Ref<Base::Object>();
    Base::Result<void> indexStored;
    if (activeProperty_ != SelectedIndexProperty) {
        indexStored = SetCurrentValue(
            SelectedIndexProperty, primaryIndex_);
    }
    if (!indexStored) {
        synchronizingProperties_ = false;
        return indexStored.GetStatus();
    }
    Base::Result<void> itemStored;
    if (activeProperty_ != SelectedItemProperty) {
        itemStored = SetCurrentValue(
            SelectedItemProperty, selected);
    }
    if (!itemStored) {
        synchronizingProperties_ = false;
        return itemStored.GetStatus();
    }
    Base::Result<void> valueStored;
    if (activeProperty_ != SelectedValueProperty) {
        valueStored = SetCurrentValue(
            SelectedValueProperty, selected);
    }
    synchronizingProperties_ = false;
    return valueStored;
}

void Selector::SyncContainers() noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    for (std::uint32_t index = 0U;
        index < generator->GeneratedCount(); ++index) {
        ItemContainer* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !PropertyRegistry().Types().IsDerivedFrom(
                container->RuntimeType(),
                ListBoxItem::StaticTypeId())) {
            continue;
        }
        auto& item =
            *static_cast<ListBoxItem*>(container);
        const bool selected = IsSelected(index);
        static_cast<void>(
            item.SetIsSelected(selected));
        if (states_ != nullptr) {
            static_cast<void>(
                states_->GoToState(
                    item,
                    "SelectionStates",
                    selected
                        ? Base::StringView("Selected")
                        : Base::StringView("Unselected")));
        }
    }
}

void Selector::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    if (pendingIndex_ != UINT32_MAX) {
        if (pendingIndex_ < ItemCount()) {
            const std::uint32_t selected =
                pendingIndex_;
            pendingIndex_ = UINT32_MAX;
            const std::uint32_t values[] = {
                selected};
            Base::Result<bool> realized =
                ApplySelection(values, selected);
            if (!realized) {
                lastSelectionError_ =
                    realized.GetStatus();
            }
        }
        return;
    }
    if (event.action == ItemsChangeAction::Reset) {
        Base::Result<bool> cleared =
            ClearSelection();
        if (!cleared) {
            lastSelectionError_ =
                cleared.GetStatus();
        }
        return;
    }
    if (event.action == ItemsChangeAction::Replace) {
        bool replacesSelection = false;
        for (std::uint32_t selected :
            selectedIndices_) {
            if (selected >= event.oldIndex &&
                selected < event.oldIndex +
                    event.oldCount) {
                replacesSelection = true;
                break;
            }
        }
        if (replacesSelection) {
            Base::Result<void> published =
                PublishProperties();
            if (!published) {
                lastSelectionError_ =
                    published.GetStatus();
            }
        }
        return;
    }

    Base::Vector<std::uint32_t> mapped;
    Base::Result<void> reserved =
        mapped.TryReserve(
            selectedIndices_.Size());
    if (!reserved) {
        lastSelectionError_ =
            reserved.GetStatus();
        return;
    }
    std::uint32_t mappedPrimary =
        primaryIndex_;
    for (std::uint32_t selected :
        selectedIndices_) {
        std::uint32_t value = selected;
        bool keep = true;
        if (event.action == ItemsChangeAction::Add &&
            selected >= event.newIndex) {
            value += event.newCount;
        } else if (
            event.action == ItemsChangeAction::Remove) {
            if (selected >= event.oldIndex &&
                selected < event.oldIndex +
                    event.oldCount) {
                keep = false;
            } else if (selected >=
                event.oldIndex + event.oldCount) {
                value -= event.oldCount;
            }
        } else if (
            event.action == ItemsChangeAction::Move &&
            event.oldCount == 1U &&
            event.newCount == 1U) {
            if (selected == event.oldIndex) {
                value = event.newIndex;
            } else if (
                event.oldIndex < event.newIndex &&
                selected > event.oldIndex &&
                selected <= event.newIndex) {
                --value;
            } else if (
                event.newIndex < event.oldIndex &&
                selected >= event.newIndex &&
                selected < event.oldIndex) {
                ++value;
            }
        }
        if (selected == primaryIndex_) {
            mappedPrimary =
                keep ? value : UINT32_MAX;
        }
        if (keep) {
            Base::Result<void> inserted =
                InsertSortedUnique(mapped, value);
            if (!inserted) {
                lastSelectionError_ =
                    inserted.GetStatus();
                return;
            }
        }
    }
    Base::Result<bool> applied =
        ApplySelection(
            {mapped.Data(), mapped.Size()},
            mappedPrimary);
    if (!applied) {
        lastSelectionError_ =
            applied.GetStatus();
    }
}

void Selector::OnPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (synchronizingProperties_) return;
    activeProperty_ = args.property;
    Base::Result<bool> applied(false);
    if (args.property == SelectionModeProperty) {
        if (GetSelectionMode() ==
                SelectionMode::Single &&
            selectedIndices_.Size() > 1U) {
            const std::uint32_t selected =
                primaryIndex_ != UINT32_MAX
                ? primaryIndex_
                : selectedIndices_[0U];
            const std::uint32_t values[] = {
                selected};
            applied = ApplySelection(
                values, selected);
        }
    } else if (args.property ==
        SelectedIndexProperty) {
        const std::uint32_t index =
            SelectedIndex();
        if (index == UINT32_MAX) {
            applied = ClearSelection();
        } else if (index >= ItemCount()) {
            pendingIndex_ = index;
            selectedIndices_.Clear();
            primaryIndex_ = UINT32_MAX;
            Base::Result<void> published =
                PublishProperties();
            if (!published) {
                lastSelectionError_ =
                    published.GetStatus();
            }
            SyncContainers();
        } else {
            const std::uint32_t values[] = {
                index};
            applied = ApplySelection(
                values, index);
        }
    } else if (
        args.property == SelectedItemProperty ||
        args.property == SelectedValueProperty) {
        const Base::Ref<Base::Object> item =
            args.property == SelectedItemProperty
            ? SelectedItem()
            : SelectedValue();
        if (!item) {
            applied = ClearSelection();
        } else {
            const std::uint32_t index =
                IndexOfItem(item.Get());
            if (index == UINT32_MAX) {
                applied = ClearSelection();
            } else {
                const std::uint32_t values[] = {
                    index};
                applied = ApplySelection(
                    values, index);
            }
        }
    }
    if (!applied) {
        lastSelectionError_ =
            applied.GetStatus();
    }
    activeProperty_ = {};
}

Base::Result<void> Selector::PrepareContainer(
    ItemContainer& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Base::Result<void> prepared =
        ItemsControl::PrepareContainer(
            container, item, index);
    if (!prepared) return prepared.GetStatus();
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        auto& listBoxItem =
            static_cast<ListBoxItem&>(container);
        Base::Result<void> selected =
            listBoxItem.SetIsSelected(
                IsSelected(index));
        if (!selected) return selected.GetStatus();
        return listBoxItem.SetTabStop(true);
    }
    return {};
}

void Selector::ClearContainer(
    ItemContainer& container) noexcept {
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        static_cast<void>(
            static_cast<ListBoxItem&>(container)
                .SetIsSelected(false));
    }
    ItemsControl::ClearContainer(container);
}

void Selector::OnContainersChanged() noexcept {
    SyncContainers();
}

ListBox::~ListBox() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            interactions_->Detach(*this));
    }
}

Base::Result<Base::Ref<ItemContainer>>
ListBox::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ListBoxItem>> made =
        Base::MakeRef<ListBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

Base::Result<bool> ListBox::BringIntoView(
    std::uint32_t index) noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr ||
        index >= generator->GeneratedCount()) {
        return false;
    }
    ItemContainer* container =
        generator->ContainerFromIndex(index);
    if (container == nullptr) return false;
    double x = 0.0;
    double y = 0.0;
    UIElement* node = container;
    ScrollViewer* viewer = nullptr;
    while (node != nullptr) {
        const Rect slot = node->LayoutSlot();
        x += slot.x;
        y += slot.y;
        Visual* parent = node->VisualParent();
        if (parent == nullptr) break;
        UIElement* parentElement =
            parent->AsUIElement();
        if (parentElement != nullptr &&
            PropertyRegistry().Types().IsDerivedFrom(
                parentElement->RuntimeType(),
                ScrollViewer::StaticTypeId())) {
            viewer =
                static_cast<ScrollViewer*>(
                    parentElement);
            break;
        }
        node = parentElement;
    }
    if (viewer == nullptr) return false;

    bool changed = false;
    const double width =
        container->RenderSize().width;
    const double height =
        container->RenderSize().height;
    double horizontal =
        viewer->HorizontalOffset();
    double vertical =
        viewer->VerticalOffset();
    if (x < 0.0) horizontal += x;
    else if (x + width >
        viewer->ViewportWidth()) {
        horizontal += x + width -
            viewer->ViewportWidth();
    }
    if (y < 0.0) vertical += y;
    else if (y + height >
        viewer->ViewportHeight()) {
        vertical += y + height -
            viewer->ViewportHeight();
    }
    Base::Result<bool> horizontalChanged =
        viewer->SetHorizontalOffset(
            std::max(0.0, horizontal));
    if (!horizontalChanged) {
        return horizontalChanged.GetStatus();
    }
    changed = horizontalChanged.Value();
    Base::Result<bool> verticalChanged =
        viewer->SetVerticalOffset(
            std::max(0.0, vertical));
    if (!verticalChanged) {
        return verticalChanged.GetStatus();
    }
    return changed || verticalChanged.Value();
}

ListBoxInteractionManager::ListBoxInteractionManager(
    ObjectTree& tree,
    RoutedEventManager& events,
    FocusManager& focus,
    VisualStateManager* states) noexcept
    : tree_(&tree),
      events_(&events),
      focus_(&focus),
      states_(states),
      mouseDownHandler_(
          this,
          &ListBoxInteractionManager::OnMouseDown),
      keyDownHandler_(
          this,
          &ListBoxInteractionManager::OnKeyDown) {}

ListBoxInteractionManager::~ListBoxInteractionManager() noexcept {
    while (!records_.Empty()) {
        ListBox* listBox =
            ResolveListBox(records_.Size() - 1U);
        if (listBox == nullptr) {
            records_.PopBack();
        } else {
            static_cast<void>(Detach(*listBox));
        }
    }
}

std::uint32_t ListBoxInteractionManager::FindListBox(
    const ListBox& listBox) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index].handle) ==
            &listBox) {
            return index;
        }
    }
    return UINT32_MAX;
}

ListBox* ListBoxInteractionManager::ResolveListBox(
    std::uint32_t index) noexcept {
    Visual* visual =
        tree_->ResolveHandle(records_[index].handle);
    return visual != nullptr
        ? static_cast<ListBox*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void> ListBoxInteractionManager::Attach(
    ListBox& listBox) noexcept {
    if (listBox.interactions_ != nullptr ||
        listBox.OwningTree() != tree_ ||
        FindListBox(listBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ListBox interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(listBox);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        listBox.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        listBox.TryAddHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    if (!key) {
        static_cast<void>(listBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        return key.GetStatus();
    }
    Record record;
    record.handle = handle.Value();
    Base::Result<void> added =
        records_.TryPushBack(record);
    if (!added) {
        static_cast<void>(listBox.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        static_cast<void>(listBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        return added.GetStatus();
    }
    listBox.interactions_ = this;
    listBox.states_ = states_;
    listBox.SyncContainers();
    return {};
}

Base::Result<bool> ListBoxInteractionManager::Detach(
    ListBox& listBox) noexcept {
    const std::uint32_t index =
        FindListBox(listBox);
    if (index == UINT32_MAX) return false;
    static_cast<void>(listBox.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(listBox.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    for (std::uint32_t current = index;
        current + 1U < records_.Size();
        ++current) {
        records_[current] =
            std::move(records_[current + 1U]);
    }
    records_.PopBack();
    listBox.interactions_ = nullptr;
    listBox.states_ = nullptr;
    return true;
}

std::uint32_t
ListBoxInteractionManager::FindContainerIndex(
    ListBox& listBox,
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !listBox.PropertyRegistry().Types()
            .IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != &listBox) {
        UIElement* element =
            visual->AsUIElement();
        if (element != nullptr &&
            listBox.PropertyRegistry().Types()
                .IsDerivedFrom(
                    element->RuntimeType(),
                    ListBoxItem::StaticTypeId())) {
            ItemContainerGenerator* generator =
                listBox.AttachedGenerator();
            return generator != nullptr
                ? generator->IndexFromContainer(
                    static_cast<ListBoxItem&>(
                        *element))
                : UINT32_MAX;
        }
        visual = visual->VisualParent();
    }
    return UINT32_MAX;
}

Base::Result<bool>
ListBoxInteractionManager::ApplyUserSelection(
    ListBox& listBox,
    Record& record,
    std::uint32_t index,
    std::uint32_t modifiers) noexcept {
    const SelectionMode mode =
        listBox.GetSelectionMode();
    if (mode == SelectionMode::Single) {
        record.anchorIndex = index;
        return listBox.SetSelectedIndex(index);
    }
    if (mode == SelectionMode::Multiple) {
        record.anchorIndex = index;
        return listBox.Toggle(index);
    }
    const bool shift = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Control);
    if (shift) {
        if (record.anchorIndex == UINT32_MAX ||
            record.anchorIndex >=
                listBox.ItemCount()) {
            record.anchorIndex =
                listBox.SelectedIndex() != UINT32_MAX
                ? listBox.SelectedIndex()
                : index;
        }
        return listBox.SelectRange(
            record.anchorIndex,
            index,
            control);
    }
    record.anchorIndex = index;
    return control
        ? listBox.Toggle(index)
        : listBox.SetSelectedIndex(index);
}

void ListBoxInteractionManager::OnMouseDown(
    Base::Object* sender,
    const MouseButtonEventArgs& args) noexcept {
    if (args.changedButton != MouseButton::Left) {
        return;
    }
    auto& listBox =
        *static_cast<ListBox*>(sender);
    if (!listBox.IsEnabled()) return;
    const std::uint32_t recordIndex =
        FindListBox(listBox);
    if (recordIndex == UINT32_MAX) return;
    const std::uint32_t index =
        FindContainerIndex(
            listBox, args.originalSource);
    if (index == UINT32_MAX) return;
    Base::Result<bool> selected =
        ApplyUserSelection(
            listBox,
            records_[recordIndex],
            index,
            args.modifiers);
    if (!selected) return;
    ItemContainerGenerator* generator =
        listBox.AttachedGenerator();
    if (generator != nullptr) {
        static_cast<void>(focus_->SetFocus(
            generator->ContainerFromIndex(index)));
    }
    static_cast<void>(
        listBox.BringIntoView(index));
    args.handled = true;
}

void ListBoxInteractionManager::OnKeyDown(
    Base::Object* sender,
    const KeyEventArgs& args) noexcept {
    if (args.key != KeyboardKeyUp &&
        args.key != KeyboardKeyDown &&
        args.key != KeyboardKeyHome &&
        args.key != KeyboardKeyEnd) {
        return;
    }
    auto& listBox =
        *static_cast<ListBox*>(sender);
    if (!listBox.IsEnabled() ||
        listBox.ItemCount() == 0U) {
        return;
    }
    const std::uint32_t recordIndex =
        FindListBox(listBox);
    if (recordIndex == UINT32_MAX) return;
    std::uint32_t current =
        FindContainerIndex(
            listBox, args.originalSource);
    if (current == UINT32_MAX) {
        current =
            listBox.SelectedIndex() != UINT32_MAX
            ? listBox.SelectedIndex()
            : 0U;
    }
    std::uint32_t target = current;
    if (args.key == KeyboardKeyUp &&
        target > 0U) {
        --target;
    } else if (args.key == KeyboardKeyDown &&
        target + 1U < listBox.ItemCount()) {
        ++target;
    } else if (args.key == KeyboardKeyHome) {
        target = 0U;
    } else if (args.key == KeyboardKeyEnd) {
        target = listBox.ItemCount() - 1U;
    }
    const bool control = HasKeyboardModifier(
        args.modifiers,
        KeyboardModifiers::Control);
    const bool shift = HasKeyboardModifier(
        args.modifiers,
        KeyboardModifiers::Shift);
    if (!control ||
        listBox.GetSelectionMode() !=
            SelectionMode::Extended ||
        shift) {
        Base::Result<bool> selected =
            ApplyUserSelection(
                listBox,
                records_[recordIndex],
                target,
                args.modifiers);
        if (!selected) return;
    }
    ItemContainerGenerator* generator =
        listBox.AttachedGenerator();
    if (generator != nullptr) {
        static_cast<void>(focus_->SetFocus(
            generator->ContainerFromIndex(target)));
    }
    static_cast<void>(
        listBox.BringIntoView(target));
    args.handled = true;
}

} // namespace Aero::Controls

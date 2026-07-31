#include "VisualStateManagerAccess.hpp"
#include <Aero/Controls/Items.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Controls/Text.hpp>
#include "ContentControlAccess.hpp"

#include "../core/metadata/BuiltinTypeIds.hpp"

#include <algorithm>
#include <utility>
#include "../ui/RuntimeManagers.hpp"
#include "RuntimeManagers.hpp"

namespace Aero::Controls {

using namespace Primitives;
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
    RoutedEventArgs routedArgs;
    Base::Result<void> routed =
        RaiseEvent(
            SelectionChangedRoutedEvent,
            &routedArgs);
    if (!routed &&
        routed.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        lastSelectionError_ =
            routed.GetStatus();
        return routed.GetStatus();
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
                Aero::Controls::Detail::VisualStateManagerAccess::GoToState(*states_,
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
            static_cast<ListBoxInteractionManager*>(
                interactions_)->Detach(*this));
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
        const Rect slot = node->GetLayoutSlot();
        x += slot.x;
        y += slot.y;
        Visual* parent = node->GetVisualParent();
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
        container->GetRenderSize().width;
    const double height =
        container->GetRenderSize().height;
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

bool ComboBoxItem::IsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

Base::Result<void> ComboBoxItem::SetIsSelected(
    bool value) noexcept {
    return SetCurrentValue(
        IsSelectedProperty, value);
}

ComboBox::ComboBox() noexcept
    : Selector(StaticTypeId()),
      selectionChangedHandler_(
          this,
          &ComboBox::OnSelectionChanged),
      dropDownChangedHandler_(
          this,
          &ComboBox::OnDropDownPropertyChanged),
      maxDropDownHeightChangedHandler_(
          this,
          &ComboBox::
              OnMaxDropDownHeightPropertyChanged),
      editableChangedHandler_(
          this,
          &ComboBox::OnEditablePropertyChanged),
      textChangedHandler_(
          this,
          &ComboBox::OnTextPropertyChanged),
      foregroundChangedHandler_(
          this,
          &ComboBox::OnForegroundPropertyChanged),
      editableTextChangedHandler_(
          this,
          &ComboBox::OnEditableTextChanged) {
    static_cast<void>(TryAddSelectionChanged(
        selectionChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsDropDownOpenProperty,
        dropDownChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        MaxDropDownHeightProperty,
        maxDropDownHeightChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsEditableProperty,
        editableChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        TextProperty,
        textChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

ComboBox::~ComboBox() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            static_cast<ComboBoxInteractionManager*>(
                interactions_)->Detach(*this));
    }
    static_cast<void>(RemoveSelectionChanged(
        selectionChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsDropDownOpenProperty,
        dropDownChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaxDropDownHeightProperty,
        maxDropDownHeightChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsEditableProperty,
        editableChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        TextProperty,
        textChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

bool ComboBox::IsDropDownOpen() const noexcept {
    return GetValueOr(
        IsDropDownOpenProperty, false);
}

Base::Result<void> ComboBox::SetIsDropDownOpen(
    bool value) noexcept {
    return SetValue(
        IsDropDownOpenProperty, value);
}

double ComboBox::MaxDropDownHeight() const noexcept {
    return GetValueOr(
        MaxDropDownHeightProperty, 240.0);
}

Base::Result<void> ComboBox::SetMaxDropDownHeight(
    double value) noexcept {
    return SetValue(
        MaxDropDownHeightProperty, value);
}

bool ComboBox::IsEditable() const noexcept {
    return GetValueOr(
        IsEditableProperty, false);
}

bool ComboBox::IsReadOnly() const noexcept {
    return GetValueOr(IsReadOnlyProperty, false);
}

Base::Result<void> ComboBox::SetIsReadOnly(
    bool value) noexcept {
    return SetValue(IsReadOnlyProperty, value);
}

Base::Result<void> ComboBox::SetIsEditable(
    bool value) noexcept {
    return SetValue(
        IsEditableProperty, value);
}

Base::StringView ComboBox::Text() const noexcept {
    return GetValueOr(
        TextProperty, Base::StringView());
}

Base::Result<void> ComboBox::SetText(
    Base::StringView value) noexcept {
    return SetValue(TextProperty, value);
}

Base::String ComboBox::SelectionBoxText() const
    noexcept {
    return GetValueOr(
        SelectionBoxTextProperty,
        Base::String{});
}

Base::Result<Base::Ref<ItemContainer>>
ComboBox::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ComboBoxItem>> made =
        Base::MakeRef<ComboBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

Base::Result<void> ComboBox::PrepareContainer(
    ItemContainer& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Base::Result<void> prepared =
        Selector::PrepareContainer(
            container, item, index);
    if (!prepared) return prepared.GetStatus();
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        auto& comboItem =
            static_cast<ComboBoxItem&>(
                container);
        Base::Result<void> selected =
            comboItem.SetIsSelected(
                IsSelected(index));
        if (!selected) {
            return selected.GetStatus();
        }
        return comboItem.SetTabStop(true);
    }
    return {};
}

void ComboBox::ClearContainer(
    ItemContainer& container) noexcept {
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        static_cast<void>(
            static_cast<ComboBoxItem&>(
                container).
                SetIsSelected(false));
    }
    Selector::ClearContainer(container);
}

void ComboBox::SynchronizeContainers() noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    for (std::uint32_t index =
             generator->FirstGeneratedIndex();
         index <
             generator->FirstGeneratedIndex() +
                 generator->GeneratedCount();
         ++index) {
        ItemContainer* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !PropertyRegistry().Types().
                IsDerivedFrom(
                    container->RuntimeType(),
                    ComboBoxItem::StaticTypeId())) {
            continue;
        }
        static_cast<void>(
            static_cast<ComboBoxItem&>(
                *container).
                SetIsSelected(
                    IsSelected(index)));
    }
}

void ComboBox::OnContainersChanged() noexcept {
    Selector::OnContainersChanged();
    SynchronizeContainers();
}

Base::Result<void> ComboBox::OnApplyTemplate()
    noexcept {
    Base::Result<void> applied =
        Selector::OnApplyTemplate();
    if (!applied) return applied.GetStatus();

    DependencyObject* selection =
        GetTemplateChild("SelectionBox");
    selectionBox_ =
        selection != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selection->RuntimeType(),
                TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(selection)
        : nullptr;
    DependencyObject* contentSite =
        GetTemplateChild("ContentSite");
    selectionPresenter_ =
        contentSite != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                contentSite->RuntimeType(),
                ContentPresenter::StaticTypeId())
        ? static_cast<ContentPresenter*>(
              contentSite)
        : nullptr;
    if (selectionBox_ == nullptr &&
        selectionPresenter_ != nullptr &&
        selectionPresenter_->Content() != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selectionPresenter_->Content()->
                    RuntimeType(),
                TextBlock::StaticTypeId())) {
        selectionBox_ =
            static_cast<TextBlock*>(
                selectionPresenter_->Content());
    }
    if (selectionBox_ != nullptr) {
        Base::Result<void> foreground =
            selectionBox_->SetForegroundBrush(
                ForegroundBrush());
        if (!foreground) {
            return foreground.GetStatus();
        }
    }
    DependencyObject* editable =
        GetTemplateChild("PART_EditableTextBox");
    editableTextBox_ =
        editable != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                editable->RuntimeType(),
                TextBox::StaticTypeId())
        ? static_cast<TextBox*>(editable)
        : nullptr;
    DependencyObject* border =
        GetTemplateChild("DropDownBorder");
    dropDownBorder_ =
        border != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                border->RuntimeType(),
                FrameworkElement::StaticTypeId())
        ? static_cast<FrameworkElement*>(border)
        : nullptr;
    DependencyObject* popup =
        GetTemplateChild("PART_Popup");
    popup_ =
        popup != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                popup->RuntimeType(),
                Popup::StaticTypeId())
        ? static_cast<Popup*>(popup)
        : nullptr;
    if ((selectionBox_ == nullptr &&
         selectionPresenter_ == nullptr) ||
        (IsEditable() &&
         editableTextBox_ == nullptr) ||
        popup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ComboBox template requires PART_Popup and a selection presenter; editable templates also require PART_EditableTextBox");
    }
    if (editableTextBox_ != nullptr) {
        Base::Result<void> editableHandler =
            editableTextBox_->TryAddHandler(
                TextBox::TextChangedEvent,
                editableTextChangedHandler_);
        if (!editableHandler) {
            return editableHandler.GetStatus();
        }
    }
    Base::Result<void> opened =
        popup_->SetIsOpen(
            IsDropDownOpen());
    if (!opened) return opened.GetStatus();
    if (dropDownBorder_ != nullptr) {
        Base::Result<void> limited =
            dropDownBorder_->SetMaxSize(
                {1.0e12,
                 MaxDropDownHeight()});
        if (!limited) {
            return limited.GetStatus();
        }
    }
    Base::Result<void> selectionUpdated =
        UpdateSelectionBox();
    if (!selectionUpdated) {
        return selectionUpdated.GetStatus();
    }
    return UpdateEditableVisualState();
}

void ComboBox::OnTemplateDetached() noexcept {
    if (editableTextBox_ != nullptr) {
        static_cast<void>(
            editableTextBox_->RemoveHandler(
                TextBox::TextChangedEvent,
                editableTextChangedHandler_));
    }
    selectionBox_ = nullptr;
    selectionPresenter_ = nullptr;
    editableTextBox_ = nullptr;
    popup_ = nullptr;
    dropDownBorder_ = nullptr;
    Selector::OnTemplateDetached();
}

void ComboBox::OnSelectionChanged(
    Selector&,
    const SelectionChangedEvent&) noexcept {
    static_cast<void>(UpdateSelectionBox());
    SynchronizeContainers();
    if (IsDropDownOpen()) {
        static_cast<void>(
            SetIsDropDownOpen(false));
    }
}

void ComboBox::OnForegroundPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (selectionBox_ != nullptr) {
        static_cast<void>(
            selectionBox_->SetForegroundBrush(
                ForegroundBrush()));
    }
    if (editableTextBox_ != nullptr) {
        static_cast<void>(
            editableTextBox_->SetForeground(
                Foreground()));
    }
}

void ComboBox::OnEditablePropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    static_cast<void>(
        UpdateEditableVisualState());
}

void ComboBox::OnTextPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    if (editableTextBox_ == nullptr ||
        synchronizingEditableText_ ||
        editableTextBox_->Text() == Text()) {
        return;
    }
    synchronizingEditableText_ = true;
    static_cast<void>(
        editableTextBox_->SetText(Text()));
    synchronizingEditableText_ = false;
}

void ComboBox::OnEditableTextChanged(
    Base::Object* sender,
    RoutedEventArgs&) noexcept {
    if (sender != editableTextBox_ ||
        editableTextBox_ == nullptr ||
        synchronizingEditableText_) {
        return;
    }
    Base::String edited;
    Base::Result<void> copied =
        edited.TryAssign(
            editableTextBox_->Text());
    if (!copied) {
        return;
    }
    synchronizingEditableText_ = true;
    static_cast<void>(
        SetCurrentValue(
            TextProperty,
            std::move(edited)));
    synchronizingEditableText_ = false;
}

void ComboBox::OnDropDownPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    if (popup_ != nullptr) {
        static_cast<void>(
            popup_->SetIsOpen(
                args.newValue.AsBoolean()));
    }
    RoutedEventArgs eventArgs;
    Base::Result<void> raised =
        RaiseEvent(
            args.newValue.AsBoolean()
                ? DropDownOpenedEvent
                : DropDownClosedEvent,
            &eventArgs);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        static_cast<void>(raised);
    }
}

void ComboBox::
OnMaxDropDownHeightPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
    noexcept {
    if (dropDownBorder_ != nullptr) {
        static_cast<void>(
            dropDownBorder_->SetMaxSize(
                {1.0e12,
                 MaxDropDownHeight()}));
    }
}

Base::Result<void>
ComboBox::UpdateSelectionBox() noexcept {
    Base::StringView text;
    Base::Ref<Base::Object> selected =
        SelectedItem();
    if (selected &&
        selected->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        const Core::Value& value =
            static_cast<const BoxedItemValue&>(
                *selected).Value();
        if (value.Kind() ==
                Core::ValueKind::String) {
            text = value.AsString();
        }
    } else if (selected &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                TextBlock::StaticTypeId())) {
        text = static_cast<TextBlock*>(
            selected.Get())->Text();
    } else if (
        selected &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                ContentControl::StaticTypeId())) {
        UIElement* content =
            Detail::ContentControlAccess::ContentElement(*static_cast<ContentControl*>(
                selected.Get()));
        if (content != nullptr &&
            PropertyRegistry().Types().
                IsDerivedFrom(
                    content->RuntimeType(),
                    TextBlock::StaticTypeId())) {
            text = static_cast<TextBlock*>(
                content)->Text();
        }
    }
    Base::String value;
    Base::Result<void> assigned =
        value.TryAssign(text);
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> published =
        SetReadOnlyCurrentValue(
            SelectionBoxTextProperty, value);
    if (!published) return published.GetStatus();
    Base::Result<Core::Value> itemValue =
        Core::Value::TryFromString(
            Core::TypeOf<Base::String>(),
            text);
    if (!itemValue) {
        return itemValue.GetStatus();
    }
    published = SetReadOnlyCurrentValue(
        SelectionBoxItemProperty,
        std::move(itemValue).Value());
    if (!published) return published.GetStatus();
    Base::Result<void> displayed =
        selectionBox_ != nullptr
        ? selectionBox_->SetText(text)
        : Base::Result<void>();
    if (!displayed) {
        return displayed.GetStatus();
    }
    if (!text.Empty()) {
        Base::Result<void> textPublished =
            SetCurrentValue(TextProperty, value);
        if (!textPublished) {
            return textPublished.GetStatus();
        }
    }
    return UpdateEditableVisualState();
}

Base::Result<void>
ComboBox::UpdateEditableVisualState() noexcept {
    if (selectionBox_ != nullptr) {
        Base::Result<void> visible =
            selectionBox_->SetVisibility(
                IsEditable()
                ? Visibility::Collapsed
                : Visibility::Visible);
        if (!visible) {
            return visible.GetStatus();
        }
    }
    if (editableTextBox_ == nullptr) {
        return {};
    }
    Base::Result<void> visible =
        editableTextBox_->SetVisibility(
            IsEditable()
            ? Visibility::Visible
            : Visibility::Collapsed);
    if (!visible) {
        return visible.GetStatus();
    }
    if (editableTextBox_->Text() == Text()) {
        return {};
    }
    synchronizingEditableText_ = true;
    Base::Result<void> updated =
        editableTextBox_->SetText(Text());
    synchronizingEditableText_ = false;
    return updated;
}

std::uint32_t ComboBox::FindContainerIndex(
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !PropertyRegistry().Types().
            IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != this) {
        UIElement* element =
            visual->AsUIElement();
        if (element != nullptr &&
            PropertyRegistry().Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    ComboBoxItem::StaticTypeId())) {
            ItemContainerGenerator* generator =
                AttachedGenerator();
            return generator != nullptr
                ? generator->IndexFromContainer(
                    static_cast<ComboBoxItem&>(
                        *element))
                : UINT32_MAX;
        }
        visual = visual->GetVisualParent();
    }
    return UINT32_MAX;
}

} // namespace Aero::Controls

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Controls;

ComboBoxInteractionManager::
ComboBoxInteractionManager(
    ObjectTree& tree,
    EventRouter& events,
    FocusManager& focus) noexcept
    : tree_(&tree),
      events_(&events),
      focus_(&focus),
      mouseDownHandler_(
          this,
          &ComboBoxInteractionManager::
              OnMouseDown),
      keyDownHandler_(
          this,
          &ComboBoxInteractionManager::
              OnKeyDown) {}

ComboBoxInteractionManager::
~ComboBoxInteractionManager() noexcept {
    while (!records_.Empty()) {
        ComboBox* comboBox =
            ResolveComboBox(
                records_.Size() - 1U);
        if (comboBox == nullptr) {
            records_.PopBack();
        } else {
            static_cast<void>(
                Detach(*comboBox));
        }
    }
}

std::uint32_t
ComboBoxInteractionManager::FindComboBox(
    const ComboBox& comboBox) const noexcept {
    for (std::uint32_t index = 0U;
         index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index]) ==
            &comboBox) {
            return index;
        }
    }
    return UINT32_MAX;
}

ComboBox*
ComboBoxInteractionManager::ResolveComboBox(
    std::uint32_t index) noexcept {
    Visual* visual =
        tree_->ResolveHandle(records_[index]);
    return visual != nullptr
        ? static_cast<ComboBox*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void>
ComboBoxInteractionManager::Attach(
    ComboBox& comboBox) noexcept {
    if (comboBox.interactions_ != nullptr ||
        Aero::Detail::VisualAccess::Tree(comboBox) != tree_ ||
        FindComboBox(comboBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ComboBox interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(comboBox);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        comboBox.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_,
            true);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        comboBox.TryAddHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    if (!key) {
        static_cast<void>(
            comboBox.RemoveHandler(
                UIElement::MouseDownEvent,
                mouseDownHandler_));
        return key.GetStatus();
    }
    Base::Result<void> stored =
        records_.TryPushBack(handle.Value());
    if (!stored) {
        static_cast<void>(
            comboBox.RemoveHandler(
                UIElement::KeyDownEvent,
                keyDownHandler_));
        static_cast<void>(
            comboBox.RemoveHandler(
                UIElement::MouseDownEvent,
                mouseDownHandler_));
        return stored.GetStatus();
    }
    comboBox.interactions_ = this;
    return {};
}

Base::Result<bool>
ComboBoxInteractionManager::Detach(
    ComboBox& comboBox) noexcept {
    const std::uint32_t index =
        FindComboBox(comboBox);
    if (index == UINT32_MAX) return false;
    static_cast<void>(
        comboBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
    static_cast<void>(
        comboBox.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
    for (std::uint32_t current = index;
         current + 1U < records_.Size();
         ++current) {
        records_[current] =
            records_[current + 1U];
    }
    records_.PopBack();
    comboBox.interactions_ = nullptr;
    return true;
}

void ComboBoxInteractionManager::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    if (args.changedButton !=
        MouseButton::Left) {
        return;
    }
    auto& comboBox =
        *static_cast<ComboBox*>(sender);
    if (!comboBox.GetIsEnabled()) return;
    const std::uint32_t index =
        comboBox.FindContainerIndex(
            args.originalSource);
    if (index != UINT32_MAX) {
        Base::Result<bool> selected =
            comboBox.SetSelectedIndex(index);
        if (!selected) return;
        static_cast<void>(
            comboBox.SetIsDropDownOpen(
                false));
    } else {
        Base::Result<void> toggled =
            comboBox.SetIsDropDownOpen(
                !comboBox.IsDropDownOpen());
        if (!toggled) return;
    }
    static_cast<void>(
        focus_->SetFocus(&comboBox));
    args.handled = true;
}

void ComboBoxInteractionManager::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& comboBox =
        *static_cast<ComboBox*>(sender);
    if (!comboBox.GetIsEnabled()) return;
    if (args.key == KeyboardKeyEscape) {
        if (!comboBox.IsDropDownOpen()) return;
        static_cast<void>(
            comboBox.SetIsDropDownOpen(
                false));
        args.handled = true;
        return;
    }
    if (args.key == KeyboardKeyEnter ||
        args.key == KeyboardKeySpace) {
        static_cast<void>(
            comboBox.SetIsDropDownOpen(
                !comboBox.
                    IsDropDownOpen()));
        args.handled = true;
        return;
    }
    if (args.key != KeyboardKeyUp &&
        args.key != KeyboardKeyDown) {
        return;
    }
    if (comboBox.ItemCount() == 0U) return;
    std::uint32_t selected =
        comboBox.SelectedIndex();
    if (selected == UINT32_MAX) {
        selected = 0U;
    } else if (
        args.key == KeyboardKeyDown &&
        selected + 1U <
            comboBox.ItemCount()) {
        ++selected;
    } else if (
        args.key == KeyboardKeyUp &&
        selected > 0U) {
        --selected;
    }
    Base::Result<bool> changed =
        comboBox.SetSelectedIndex(selected);
    if (!changed) return;
    args.handled = true;
}

ListBoxInteractionManager::ListBoxInteractionManager(
    ObjectTree& tree,
    EventRouter& events,
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
        Aero::Detail::VisualAccess::Tree(listBox) != tree_ ||
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
        visual = visual->GetVisualParent();
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
    MouseButtonEventArgs& args) noexcept {
    if (args.changedButton != MouseButton::Left) {
        return;
    }
    auto& listBox =
        *static_cast<ListBox*>(sender);
    if (!listBox.GetIsEnabled()) return;
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
    KeyEventArgs& args) noexcept {
    if (args.key != KeyboardKeyUp &&
        args.key != KeyboardKeyDown &&
        args.key != KeyboardKeyHome &&
        args.key != KeyboardKeyEnd) {
        return;
    }
    auto& listBox =
        *static_cast<ListBox*>(sender);
    if (!listBox.GetIsEnabled() ||
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

} // namespace Aero::Detail

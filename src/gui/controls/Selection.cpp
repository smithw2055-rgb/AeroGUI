#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/core/facets/InteractionStateFacet.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>


#include <algorithm>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::ComboBehavior;
using Aero::Controls::ListBehavior;

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
        values.Reserve(values.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> appended =
        values.PushBack(value);
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

bool ListBoxItem::GetIsSelected() const noexcept {
    return GetValueOr(IsSelectedProperty, false);
}

void ListBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

Selector::Selector() noexcept
    : Selector(StaticTypeId()) {}

Selector::Selector(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType),
      itemsChangedHandler_(
          this, &Selector::OnItemsChanged),
      propertyChangedHandler_(
          this, &Selector::OnPropertyChanged) {
    AddItemsChanged(itemsChangedHandler_);
    static_cast<void>(AddValueChangedHandlerChecked(
        SelectionModeProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        SelectedIndexProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        SelectedItemProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
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

std::uint32_t Selector::GetSelectedIndex() const noexcept {
    return GetValueOr(SelectedIndexProperty, UINT32_MAX);
}

Base::Ref<Base::Object>
Selector::GetSelectedItem() const noexcept {
    return GetValueOr(
        SelectedItemProperty,
        Base::Ref<Base::Object>{});
}

Base::Ref<Base::Object>
Selector::GetSelectedValue() const noexcept {
    return GetValueOr(
        SelectedValueProperty,
        Base::Ref<Base::Object>{});
}

bool Selector::GetIsSelected(
    std::uint32_t index) const noexcept {
    return ContainsIndex(
        {selectedIndices_.Data(),
         selectedIndices_.Size()},
        index);
}

std::uint32_t Selector::GetIndexOfItem(
    const Base::Object* item) const noexcept {
    if (item == nullptr) return UINT32_MAX;
    for (std::uint32_t index = 0U;
        index < GetCount(); ++index) {
        if (GetItem(index).Get() == item) return index;
    }
    return UINT32_MAX;
}

void Selector::SetSelectionMode(
    SelectionMode value) noexcept {
    lastSelectionError_ = {};
    SetValue(SelectionModeProperty, value);
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
            lastSelectionError_ = normalized.GetStatus();
        }
    }
}

void Selector::SetSelectedIndex(
    std::uint32_t index) noexcept {
    if (index != UINT32_MAX &&
        index >= GetCount()) {
        lastSelectionError_ = Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector selected index is out of range");
        return;
    }
    if (index == UINT32_MAX) {
        ClearSelection();
        return;
    }
    pendingSelectedItem_.Reset();
    const std::uint32_t values[] = {index};
    Base::Result<bool> result = ApplySelection(values, index);
    if (!result) {
        lastSelectionError_ = result.GetStatus();
    } else {
        lastSelectionError_ = Base::Status::Ok();
    }
}

void Selector::SetSelectedItem(
    Base::Ref<Base::Object> item) noexcept {
    if (!item) {
        ClearSelection();
        return;
    }
    const std::uint32_t index =
        GetIndexOfItem(item.Get());
    if (index == UINT32_MAX) {
        pendingSelectedItem_ = std::move(item);
        lastSelectionError_ = Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Selector selected item is pending ItemsSource materialization");
        return;
    }
    pendingSelectedItem_.Reset();
    SetSelectedIndex(index);
}

void Selector::SetSelectedValue(
    Base::Ref<Base::Object> value) noexcept {
    SetSelectedItem(std::move(value));
}

bool Selector::Select(
    std::uint32_t index) noexcept {
    if (index >= GetCount()) {
        lastSelectionError_ = Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector select index is out of range");
        return false;
    }
    if (GetSelectionMode() == SelectionMode::Single) {
        const std::uint32_t values[] = {index};
        Base::Result<bool> result = ApplySelection(values, index);
        if (!result) {
            lastSelectionError_ = result.GetStatus();
            return false;
        }
        lastSelectionError_ = Base::Status::Ok();
        return result.Value();
    }
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.Reserve(
            selectedIndices_.Size() + 1U);
    if (!reserved) {
        lastSelectionError_ = reserved.GetStatus();
        return false;
    }
    for (std::uint32_t selected :
        selectedIndices_) {
        Base::Result<void> copied =
            selection.PushBack(selected);
        if (!copied) {
            lastSelectionError_ = copied.GetStatus();
            return false;
        }
    }
    Base::Result<void> inserted =
        InsertSortedUnique(selection, index);
    if (!inserted) {
        lastSelectionError_ = inserted.GetStatus();
        return false;
    }
    Base::Result<bool> result = ApplySelection(
        {selection.Data(), selection.Size()},
        index);
    if (!result) {
        lastSelectionError_ = result.GetStatus();
        return false;
    }
    lastSelectionError_ = Base::Status::Ok();
    return result.Value();
}

bool Selector::Unselect(
    std::uint32_t index) noexcept {
    if (index >= GetCount()) {
        lastSelectionError_ = Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector unselect index is out of range");
        return false;
    }
    if (!GetIsSelected(index)) {
        lastSelectionError_ = Base::Status::Ok();
        return false;
    }
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.Reserve(
            selectedIndices_.Size() - 1U);
    if (!reserved) {
        lastSelectionError_ = reserved.GetStatus();
        return false;
    }
    for (std::uint32_t selected :
        selectedIndices_) {
        if (selected == index) continue;
        Base::Result<void> copied =
            selection.PushBack(selected);
        if (!copied) {
            lastSelectionError_ = copied.GetStatus();
            return false;
        }
    }
    const std::uint32_t primary =
        primaryIndex_ != index
        ? primaryIndex_
        : (selection.Empty()
            ? UINT32_MAX
            : selection.Back());
    Base::Result<bool> result = ApplySelection(
        {selection.Data(), selection.Size()},
        primary);
    if (!result) {
        lastSelectionError_ = result.GetStatus();
        return false;
    }
    lastSelectionError_ = Base::Status::Ok();
    return result.Value();
}

bool Selector::Toggle(
    std::uint32_t index) noexcept {
    return GetIsSelected(index)
        ? Unselect(index)
        : Select(index);
}

bool Selector::SelectRange(
    std::uint32_t first,
    std::uint32_t last,
    bool preserveExisting) noexcept {
    if (first >= GetCount() ||
        last >= GetCount()) {
        lastSelectionError_ = Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Selector range is out of range");
        return false;
    }
    if (GetSelectionMode() == SelectionMode::Single) {
        const std::uint32_t values[] = {last};
        Base::Result<bool> result = ApplySelection(values, last);
        if (!result) {
            lastSelectionError_ = result.GetStatus();
            return false;
        }
        lastSelectionError_ = Base::Status::Ok();
        return result.Value();
    }
    const std::uint32_t begin =
        std::min(first, last);
    const std::uint32_t end =
        std::max(first, last);
    Base::Vector<std::uint32_t> selection;
    Base::Result<void> reserved =
        selection.Reserve(
            (preserveExisting
                ? selectedIndices_.Size()
                : 0U) +
            (end - begin + 1U));
    if (!reserved) {
        lastSelectionError_ = reserved.GetStatus();
        return false;
    }
    if (preserveExisting) {
        for (std::uint32_t selected :
            selectedIndices_) {
            Base::Result<void> copied =
                selection.PushBack(selected);
            if (!copied) {
                lastSelectionError_ = copied.GetStatus();
                return false;
            }
        }
    }
    for (std::uint32_t index = begin;
        index <= end; ++index) {
        Base::Result<void> inserted =
            InsertSortedUnique(selection, index);
        if (!inserted) {
            lastSelectionError_ = inserted.GetStatus();
            return false;
        }
    }
    Base::Result<bool> result = ApplySelection(
        {selection.Data(), selection.Size()},
        last);
    if (!result) {
        lastSelectionError_ = result.GetStatus();
        return false;
    }
    lastSelectionError_ = Base::Status::Ok();
    return result.Value();
}

void Selector::ClearSelection() noexcept {
    pendingSelectedItem_.Reset();
    Base::Result<bool> result = ApplySelection({}, UINT32_MAX);
    if (!result) {
        lastSelectionError_ = result.GetStatus();
    } else {
        lastSelectionError_ = Base::Status::Ok();
    }
}

Base::Result<bool> Selector::ApplySelection(
    Base::Span<const std::uint32_t> indices,
    std::uint32_t primaryIndex) noexcept {
    Base::Vector<std::uint32_t> normalized;
    Base::Result<void> reserved =
        normalized.Reserve(indices.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index : indices) {
        if (index >= GetCount()) {
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
            normalized.PushBack(selected);
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
        removed.Reserve(selectedIndices_.Size());
    if (!removedReserve) {
        return removedReserve.GetStatus();
    }
    Base::Result<void> addedReserve =
        added.Reserve(normalized.Size());
    if (!addedReserve) return addedReserve.GetStatus();
    for (std::uint32_t index : selectedIndices_) {
        if (!ContainsIndex(newSelection, index)) {
            Base::Result<void> stored =
                removed.PushBack(index);
            if (!stored) return stored.GetStatus();
        }
    }
    for (std::uint32_t index : normalized) {
        if (!ContainsIndex(oldSelection, index)) {
            Base::Result<void> stored =
                added.PushBack(index);
            if (!stored) return stored.GetStatus();
        }
    }

    const std::uint32_t oldPrimary =
        primaryIndex_;
    Base::Ref<Base::Object> oldPrimaryItem =
        oldPrimary < GetCount()
        ? GetItem(oldPrimary)
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
            primaryIndex_ < GetCount()
            ? GetItem(primaryIndex_)
            : Base::Ref<Base::Object>();
        selectionChanged_.Invoke(*this, event);
    }
    RoutedEventArgs routedArgs;
    RaiseEvent(
        SelectionChangedRoutedEvent,
        &routedArgs);
    lastSelectionError_ = {};
    return true;
}

Base::Result<void> Selector::PublishProperties() noexcept {
    synchronizingProperties_ = true;
    const Base::Ref<Base::Object> selected =
        primaryIndex_ < GetCount()
        ? GetItem(primaryIndex_)
        : Base::Ref<Base::Object>();
    if (activeProperty_ != SelectedIndexProperty) {
        SetCurrentValue(SelectedIndexProperty, primaryIndex_);
    }
    if (activeProperty_ != SelectedItemProperty) {
        SetCurrentValue(SelectedItemProperty, selected);
    }
    if (activeProperty_ != SelectedValueProperty) {
        SetCurrentValue(SelectedValueProperty, selected);
    }
    synchronizingProperties_ = false;
    return {};
}

void Selector::SyncContainers() noexcept {
    auto* states = static_cast<Aero::VisualStateManager*>(
        ::Aero::Core::InteractionStateFacet::VisualStateRuntime(*this));
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    for (std::uint32_t index = 0U;
        index < generator->GetGeneratedCount(); ++index) {
        FrameworkElement* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !PropertyRegistry().Types().IsDerivedFrom(
                container->RuntimeType(),
                ListBoxItem::StaticTypeId())) {
            continue;
        }
        auto& item =
            *static_cast<ListBoxItem*>(container);
        const bool selected = GetIsSelected(index);
        item.SetIsSelected(selected);
        if (states != nullptr) {
            static_cast<void>(
                Aero::Controls::TemplatePrivate::GoToState(*states,
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
    if (pendingSelectedItem_) {
        const std::uint32_t index = GetIndexOfItem(
            pendingSelectedItem_.Get());
        if (index != UINT32_MAX) {
            pendingSelectedItem_.Reset();
            const std::uint32_t values[] = {index};
            Base::Result<bool> realized = ApplySelection(values, index);
            if (!realized) {
                lastSelectionError_ = realized.GetStatus();
            }
            return;
        }
        // A bound ItemsSource frequently emits Reset before it emits the
        // populated collection. Keep the requested object through that
        // transition rather than clearing the TwoWay SelectedItem source.
        if (event.action == ItemsChangeAction::Reset) {
            selectedIndices_.Clear();
            primaryIndex_ = UINT32_MAX;
            return;
        }
    }
    if (pendingIndex_ != UINT32_MAX) {
        if (pendingIndex_ < GetCount()) {
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
        ClearSelection();
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
        mapped.Reserve(
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
    activeProperty_ = args.GetProperty();
    Base::Result<bool> applied = false;
    if (args.GetProperty() == SelectionModeProperty) {
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
    } else if (args.GetProperty() ==
        SelectedIndexProperty) {
        const std::uint32_t index =
            GetSelectedIndex();
        if (index == UINT32_MAX) {
            ClearSelection();
            applied = true;
        } else if (index >= GetCount()) {
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
        args.GetProperty() == SelectedItemProperty ||
        args.GetProperty() == SelectedValueProperty) {
        const Base::Ref<Base::Object> item =
            args.GetProperty() == SelectedItemProperty
            ? GetSelectedItem()
            : GetSelectedValue();
        if (!item) {
            ClearSelection();
            applied = true;
        } else {
            const std::uint32_t index =
                GetIndexOfItem(item.Get());
            if (index == UINT32_MAX) {
                pendingSelectedItem_ = item;
                selectedIndices_.Clear();
                primaryIndex_ = UINT32_MAX;
                applied = true;
            } else {
                pendingSelectedItem_.Reset();
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
    FrameworkElement& container,
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
        listBoxItem.SetIsSelected(GetIsSelected(index));
        listBoxItem.SetIsTabStop(true);
        return {};
    }
    return {};
}

void Selector::ClearContainer(
    FrameworkElement& container) noexcept {
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        static_cast<ListBoxItem&>(container).SetIsSelected(false);
    }
    ItemsControl::ClearContainer(container);
}

void Selector::OnContainersChanged() noexcept {
    SyncContainers();
}

ListBox::~ListBox() {
    auto* behaviors = static_cast<ControlBehavior*>(
        ::Aero::Core::InteractionStateFacet::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->Detach(*this));
    }
}

Base::Result<Base::Ref<FrameworkElement>>
ListBox::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ListBoxItem>> made =
        Base::MakeRef<ListBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<bool> ListBox::BringIntoView(
    std::uint32_t index) noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr ||
        index >= generator->GetGeneratedCount()) {
        return false;
    }
    FrameworkElement* container =
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
        ::Aero::Media::Visual* parent = node->GetVisualParent();
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
        viewer->GetHorizontalOffset();
    double vertical =
        viewer->GetVerticalOffset();
    if (x < 0.0) horizontal += x;
    else if (x + width >
        viewer->GetViewportWidth()) {
        horizontal += x + width -
            viewer->GetViewportWidth();
    }
    if (y < 0.0) vertical += y;
    else if (y + height >
        viewer->GetViewportHeight()) {
        vertical += y + height -
            viewer->GetViewportHeight();
    }
    const double oldHorizontal = viewer->GetHorizontalOffset();
    const double oldVertical = viewer->GetVerticalOffset();
    viewer->SetHorizontalOffset(std::max(0.0, horizontal));
    viewer->SetVerticalOffset(std::max(0.0, vertical));
    changed = oldHorizontal != viewer->GetHorizontalOffset();
    return changed || oldVertical != viewer->GetVerticalOffset();
}

bool ComboBoxItem::GetIsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

void ComboBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

ComboBox::ComboBox() noexcept
    : Selector(StaticTypeId()),
      selectionChangedHandler_(
          this,
          &ComboBox::OnSelectionChanged),
      dropDownChangedHandler_(
          this,
          &ComboBox::OnDropDownPropertyChanged),
      popupIsOpenChangedHandler_(
          this,
          &ComboBox::OnPopupIsOpenChanged),
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
      selectedValueChangedHandler_(
          this,
          &ComboBox::OnSelectedValuePropertyChanged),
      selectedProjectionChangedHandler_(
          this,
          &ComboBox::OnSelectedProjectionChanged),
      editableTextChangedHandler_(
          this,
          &ComboBox::OnEditableTextChanged) {
    static_cast<void>(AddSelectionChanged(
        selectionChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsDropDownOpenProperty,
        dropDownChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        MaxDropDownHeightProperty,
        maxDropDownHeightChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsEditableProperty,
        editableChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        TextProperty,
        textChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        Selector::SelectedIndexProperty,
        selectedValueChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        Selector::SelectedItemProperty,
        selectedValueChangedHandler_));
}

ComboBox::~ComboBox() {
    ObserveSelectedProjection(nullptr);
    auto* behaviors = static_cast<ControlBehavior*>(
        ::Aero::Core::InteractionStateFacet::ControlBehaviorRuntime(*this));
    if (behaviors != nullptr) {
        static_cast<void>(behaviors->Detach(*this));
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
    static_cast<void>(RemoveValueChangedHandler(
        Selector::SelectedIndexProperty,
        selectedValueChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        Selector::SelectedItemProperty,
        selectedValueChangedHandler_));
}

bool ComboBox::GetIsDropDownOpen() const noexcept {
    return GetValueOr(
        IsDropDownOpenProperty, false);
}

void ComboBox::SetIsDropDownOpen(
    bool value) noexcept {
    SetValue(IsDropDownOpenProperty, value);
}

double ComboBox::GetMaxDropDownHeight() const noexcept {
    return GetValueOr(
        MaxDropDownHeightProperty, 240.0);
}

void ComboBox::SetMaxDropDownHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(MaxDropDownHeightProperty, value);
}

bool ComboBox::GetIsEditable() const noexcept {
    return GetValueOr(
        IsEditableProperty, false);
}

bool ComboBox::GetIsReadOnly() const noexcept {
    return GetValueOr(IsReadOnlyProperty, false);
}

void ComboBox::SetIsReadOnly(
    bool value) noexcept {
    SetValue(IsReadOnlyProperty, value);
}

void ComboBox::SetIsEditable(
    bool value) noexcept {
    SetValue(IsEditableProperty, value);
}

Base::StringView ComboBox::GetText() const noexcept {
    return GetValueOr(
        TextProperty, Base::StringView());
}

void ComboBox::SetText(
    Base::StringView value) noexcept {
    SetValue(TextProperty, value);
}

Base::String ComboBox::GetSelectionBoxText() const
    noexcept {
    return GetValueOr(
        SelectionBoxTextProperty,
        Base::String{});
}

Base::Result<Base::Ref<FrameworkElement>>
ComboBox::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ComboBoxItem>> made =
        Base::MakeRef<ComboBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<void> ComboBox::PrepareContainer(
    FrameworkElement& container,
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
        comboItem.SetIsSelected(GetIsSelected(index));
        comboItem.SetIsTabStop(true);
        return {};
    }
    return {};
}

void ComboBox::ClearContainer(
    FrameworkElement& container) noexcept {
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        static_cast<ComboBoxItem&>(container).SetIsSelected(false);
    }
    Selector::ClearContainer(container);
}

void ComboBox::SynchronizeContainers() noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    for (std::uint32_t index =
             generator->GetFirstGeneratedIndex();
         index <
             generator->GetFirstGeneratedIndex() +
                 generator->GetGeneratedCount();
         ++index) {
        FrameworkElement* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !PropertyRegistry().Types().
                IsDerivedFrom(
                    container->RuntimeType(),
                    ComboBoxItem::StaticTypeId())) {
            continue;
        }
        static_cast<ComboBoxItem&>(*container).SetIsSelected(GetIsSelected(index));
    }
}

void ComboBox::OnContainersChanged() noexcept {
    Selector::OnContainersChanged();
    SynchronizeContainers();
    // Popup item containers are often generated after the closed presenter.
    // Once the full template exists, reuse their ItemTemplate projection for
    // the selected model item.
    if (popup_ != nullptr) {
        static_cast<void>(UpdateSelectionBox());
    }
}

void ComboBox::OnApplyTemplate()
    noexcept {
    Selector::OnApplyTemplate();

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
        selectionPresenter_->GetContent() != nullptr &&
        PropertyRegistry().Types().
            IsDerivedFrom(
        selectionPresenter_->GetContent()->
                    RuntimeType(),
                TextBlock::StaticTypeId())) {
        selectionBox_ =
            static_cast<TextBlock*>(
                selectionPresenter_->GetContent());
    }
    if (selectionBox_ != nullptr) {
        selectionBox_->SetForeground(GetForeground());
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
    if (editableTextBox_ != nullptr) {
        Base::Result<void> editableHandler =
            editableTextBox_->AddHandlerChecked(
                TextBox::TextChangedEvent,
                editableTextChangedHandler_);
        if (!editableHandler) {
            return;
        }
    }
    if (popup_ != nullptr) {
        static_cast<void>(popup_->AddValueChangedHandlerChecked(
            Popup::IsOpenProperty,
            popupIsOpenChangedHandler_));
        popup_->SetPlacementTarget(
            Base::Ref<UIElement>::TryFromBorrowed(*this));
        popup_->SetStaysOpen(false);
        popup_->SetIsOpen(GetIsDropDownOpen());
        popup_->SetMatchPlacementTargetWidth(true);
    }
    if (dropDownBorder_ != nullptr) {
        dropDownBorder_->SetMaxSize({1.0e12, GetMaxDropDownHeight()});
    }
    Base::Result<void> selectionUpdated =
        UpdateSelectionBox();
    if (!selectionUpdated) {
        return;
    }
    static_cast<void>(UpdateEditableVisualState());
}

void ComboBox::OnTemplateDetached() noexcept {
    ObserveSelectedProjection(nullptr);
    if (editableTextBox_ != nullptr) {
        static_cast<void>(
            editableTextBox_->RemoveHandler(
                TextBox::TextChangedEvent,
                editableTextChangedHandler_));
    }
    if (popup_ != nullptr) {
        static_cast<void>(popup_->RemoveValueChangedHandler(
            Popup::IsOpenProperty,
            popupIsOpenChangedHandler_));
        popup_->SetPlacementTarget({});
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
    if (GetIsDropDownOpen()) {
        SetIsDropDownOpen(false);
    }
}

void ComboBox::OnForegroundPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (selectionBox_ != nullptr) {
        selectionBox_->SetForeground(GetForeground());
    }
    if (editableTextBox_ != nullptr) {
        editableTextBox_->SetForeground(GetForeground());
    }
}

void ComboBox::OnSelectedValuePropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    // SelectedItem can be supplied before an ItemsSource has materialized.
    // The later SelectedIndex publication is the point at which the closed
    // presenter must refresh, even when the SelectedItem reference itself
    // did not change.
    static_cast<void>(UpdateSelectionBox());
}

void ComboBox::OnSelectedProjectionChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (&object == selectedProjection_) {
        static_cast<void>(UpdateSelectionBox());
    }
}

void ComboBox::ObserveSelectedProjection(
    TextBlock* projection) noexcept {
    if (selectedProjection_ == projection) return;
    if (selectedProjection_ != nullptr) {
        static_cast<void>(selectedProjection_->RemoveValueChangedHandler(
            TextBlock::TextProperty,
            selectedProjectionChangedHandler_));
    }
    selectedProjection_ = projection;
    if (selectedProjection_ != nullptr) {
        Base::Result<void> observed =
            selectedProjection_->AddValueChangedHandlerChecked(
                TextBlock::TextProperty,
                selectedProjectionChangedHandler_);
        if (!observed) selectedProjection_ = nullptr;
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
        editableTextBox_->GetText() == GetText()) {
        return;
    }
    synchronizingEditableText_ = true;
    editableTextBox_->SetText(GetText());
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
        edited.Assign(
            editableTextBox_->GetText());
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
    if (popup_ != nullptr && popup_->GetIsOpen() != args.GetNewValue().AsBoolean()) {
        static_cast<void>(
            popup_->SetIsOpen(
                args.GetNewValue().AsBoolean()));
    }
    RoutedEventArgs eventArgs;
    RaiseEvent(
        args.GetNewValue().AsBoolean()
            ? DropDownOpenedEvent
            : DropDownClosedEvent,
        &eventArgs);
}

void ComboBox::OnPopupIsOpenChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    const bool open = args.GetNewValue().AsBoolean();
    if (open != GetIsDropDownOpen()) {
        SetIsDropDownOpen(open);
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
                 GetMaxDropDownHeight()}));
    }
}

Base::Result<void>
ComboBox::UpdateSelectionBox() noexcept {
    Base::StringView text;
    TextBlock* selectedProjection = nullptr;
    Base::Ref<Base::Object> selected =
        GetSelectedItem();
    if (selected &&
        selected->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        const Meta::Value& value =
            static_cast<const BoxedItemValue&>(
                *selected).Value();
        if (value.Kind() ==
                Meta::ValueKind::String) {
            text = value.AsString();
        }
    } else if (selected &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                TextBlock::StaticTypeId())) {
        text = static_cast<TextBlock*>(
            selected.Get())->GetText();
    } else if (
        selected &&
        PropertyRegistry().Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                ContentControl::StaticTypeId())) {
        UIElement* content =
            ::Aero::Core::InteractionStateFacet::ContentElement(*static_cast<ContentControl*>(
                selected.Get()));
        if (content != nullptr &&
            PropertyRegistry().Types().
                IsDerivedFrom(
                    content->RuntimeType(),
                    TextBlock::StaticTypeId())) {
            text = static_cast<TextBlock*>(
                content)->GetText();
        }
    }
    if (text.Empty() && selected) {
        // Data items are displayed through their existing ItemTemplate. This
        // is the same presentation the popup list uses and avoids a
        // Localization-specific model branch in ComboBox.
        ItemContainerGenerator* generator = AttachedGenerator();
        const std::uint32_t index = GetSelectedIndex();
        FrameworkElement* container =
            generator != nullptr && index != UINT32_MAX
            ? generator->ContainerFromIndex(index)
            : nullptr;
        if (container != nullptr &&
            PropertyRegistry().Types().IsDerivedFrom(
                container->RuntimeType(),
                ContentControl::StaticTypeId())) {
            UIElement* content = ::Aero::Core::InteractionStateFacet::ContentElement(
                *static_cast<ContentControl*>(container));
            if (content != nullptr &&
                PropertyRegistry().Types().IsDerivedFrom(
                    content->RuntimeType(), TextBlock::StaticTypeId())) {
                selectedProjection = static_cast<TextBlock*>(content);
                text = selectedProjection->GetText();
            }
        }
    }
    ObserveSelectedProjection(selectedProjection);
    if (text.Empty() && selected) {
        // When an ItemTemplate is realized after the initial selection, use
        // its conventional display property as a generic closed-state
        // fallback. This keeps model objects out of ComboBox while matching
        // WPF's selected-item presentation timing.
        Meta::ObjectFactoryState services = Meta::CurrentObjectFactory();
        const Meta::PropertyInfo* name = services.metadata != nullptr
            ? services.metadata->Types().FindProperty(
                selected->RuntimeType(), Base::StringView("Name"), true)
            : nullptr;
        if (name != nullptr) {
            Base::Result<Meta::Value> displayed =
                services.metadata->GetProperty(*selected, name->Id());
            if (displayed &&
                displayed.Value().Kind() == Meta::ValueKind::String) {
                text = displayed.Value().AsString();
            }
        }
    }
    Base::String value;
    Base::Result<void> assigned =
        value.Assign(text);
    if (!assigned) return assigned.GetStatus();
    SetReadOnlyCurrentValue(SelectionBoxTextProperty, value);
    Base::Result<Meta::Value> itemValue =
        Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(),
            text);
    if (!itemValue) {
        return itemValue.GetStatus();
    }
    Meta::Value selectionItem = std::move(itemValue).Value();
    SetReadOnlyCurrentValue(SelectionBoxItemProperty, selectionItem);
    // ContentSource is compiled into a TemplateBinding, but the closed
    // presenter is constructed before ItemsSource has materialized its first
    // selection.  Feed its current content at the same point as the
    // read-only source update so the initial selection is visible without
    // waiting for another template application or user selection change.
    if (selectionPresenter_ != nullptr) {
        selectionPresenter_->SetContentValue(selectionItem);
    }
    if (selectionBox_ != nullptr) {
        selectionBox_->SetText(text);
    }
    if (!text.Empty()) {
        SetCurrentValue(TextProperty, value);
    }
    return UpdateEditableVisualState();
}

Base::Result<void>
ComboBox::UpdateEditableVisualState() noexcept {
    if (selectionBox_ != nullptr) {
        selectionBox_->SetVisibility(GetIsEditable()
            ? Visibility::Collapsed : Visibility::Visible);
    }
    if (editableTextBox_ == nullptr) {
        return {};
    }
    editableTextBox_->SetVisibility(GetIsEditable()
        ? Visibility::Visible : Visibility::Collapsed);
    if (editableTextBox_->GetText() == GetText()) {
        return {};
    }
    synchronizingEditableText_ = true;
    editableTextBox_->SetText(GetText());
    synchronizingEditableText_ = false;
    return {};
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
    ::Aero::Media::Visual* visual =
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

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

ComboBehavior::
ComboBehavior(
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
          &ComboBehavior::
              OnMouseDown),
      keyDownHandler_(
          this,
          &ComboBehavior::
              OnKeyDown),
      pointerStateChangedHandler_(
          this,
          &ComboBehavior::
              OnPointerStateChanged) {}

ComboBehavior::
~ComboBehavior() noexcept {
    if (input_ != nullptr) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
    }
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
ComboBehavior::FindComboBox(
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
ComboBehavior::ResolveComboBox(
    std::uint32_t index) noexcept {
    ::Aero::Media::Visual* visual =
        tree_->ResolveHandle(records_[index]);
    return visual != nullptr
        ? static_cast<ComboBox*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void>
ComboBehavior::Attach(
    ComboBox& comboBox) noexcept {
    if (comboBox.GetTree() != tree_ ||
        FindComboBox(comboBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ComboBox interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(comboBox);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        comboBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_,
            true);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        comboBox.AddHandlerChecked(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    if (!key) {
        static_cast<void>(
            comboBox.RemoveHandler(
                UIElement::MouseDownEvent,
                mouseDownHandler_));
        return key.GetStatus();
    }
    if (records_.Empty() && input_ != nullptr) {
        static_cast<void>(
            input_->AddPointerStateChanged(
                pointerStateChangedHandler_));
    }
    Base::Result<void> stored =
        records_.PushBack(handle.Value());
    if (!stored) {
        if (records_.Empty() && input_ != nullptr) {
            static_cast<void>(
                input_->RemovePointerStateChanged(
                    pointerStateChangedHandler_));
        }
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
    return {};
}

Base::Result<bool>
ComboBehavior::Detach(
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
    if (records_.Empty() && input_ != nullptr) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
    }
    return true;
}

void ComboBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    if (args.GetChangedButton() !=
        MouseButton::Left) {
        return;
    }
    auto& comboBox =
        *static_cast<ComboBox*>(sender);
    if (!comboBox.GetIsEnabled()) return;
    const std::uint32_t index =
        comboBox.FindContainerIndex(
            args.GetOriginalSource());
    if (index != UINT32_MAX) {
        comboBox.SetSelectedIndex(index);
        comboBox.SetIsDropDownOpen(false);
    } else {
        comboBox.SetIsDropDownOpen(!comboBox.GetIsDropDownOpen());
    }
    static_cast<void>(
        input_->SetFocus(&comboBox));
    args.SetHandled(true);
}

void ComboBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& comboBox =
        *static_cast<ComboBox*>(sender);
    if (!comboBox.GetIsEnabled()) return;
    if (args.GetKey() == KeyboardKeyEscape) {
        if (!comboBox.GetIsDropDownOpen()) return;
        comboBox.SetIsDropDownOpen(false);
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyEnter ||
        args.GetKey() == KeyboardKeySpace) {
        comboBox.SetIsDropDownOpen(!comboBox.GetIsDropDownOpen());
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown) {
        return;
    }
    if (comboBox.GetCount() == 0U) return;
    std::uint32_t selected =
        comboBox.GetSelectedIndex();
    if (selected == UINT32_MAX) {
        selected = 0U;
    } else if (
        args.GetKey() == KeyboardKeyDown &&
        selected + 1U <
            comboBox.GetCount()) {
        ++selected;
    } else if (
        args.GetKey() == KeyboardKeyUp &&
        selected > 0U) {
        --selected;
    }
    comboBox.SetSelectedIndex(selected);
    args.SetHandled(true);
}

void ComboBehavior::OnPointerStateChanged(
    UIElement& element) noexcept {
    if (states_ == nullptr) return;
    for (std::uint32_t i = 0U; i < records_.Size(); ++i) {
        ComboBox* comboBox = ResolveComboBox(i);
        if (comboBox == nullptr) continue;

        Base::StringView comboCommon = "Normal";
        if (!comboBox->GetIsEnabled()) {
            comboCommon = "Disabled";
        } else if (comboBox->GetIsMouseOver()) {
            comboCommon = "MouseOver";
        }
        static_cast<void>(
            Aero::Controls::TemplatePrivate::GoToState(
                *states_,
                *comboBox,
                "CommonStates",
                comboCommon,
                true));

        const std::uint32_t index =
            comboBox->FindContainerIndex(&element);
        if (index != UINT32_MAX) {
            ItemContainerGenerator* generator =
                comboBox->GetItemContainerGenerator();
            if (generator != nullptr) {
                FrameworkElement* container =
                    generator->ContainerFromIndex(index);
                if (container != nullptr &&
                    comboBox->PropertyRegistry().Types().IsDerivedFrom(
                        container->RuntimeType(),
                        ComboBoxItem::StaticTypeId())) {
                    auto& item =
                        *static_cast<ComboBoxItem*>(container);
                    Base::StringView common = "Normal";
                    if (!item.GetIsEnabled()) {
                        common = "Disabled";
                    } else if (item.GetIsMouseOver()) {
                        common = "MouseOver";
                    }
                    static_cast<void>(
                        Aero::Controls::TemplatePrivate::GoToState(
                            *states_,
                            item,
                            "CommonStates",
                            common,
                            true));
                    const bool selected = item.GetIsSelected();
                    static_cast<void>(
                        Aero::Controls::TemplatePrivate::GoToState(
                            *states_,
                            item,
                            "SelectionStates",
                            selected
                                ? Base::StringView("Selected")
                                : Base::StringView("Unselected"),
                            true));
                }
            }
        }
    }
}

ListBehavior::ListBehavior(
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
          &ListBehavior::OnMouseDown),
      keyDownHandler_(
          this,
          &ListBehavior::OnKeyDown),
      pointerStateChangedHandler_(
          this,
          &ListBehavior::OnPointerStateChanged) {}

ListBehavior::~ListBehavior() noexcept {
    if (input_ != nullptr) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
    }
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

std::uint32_t ListBehavior::FindListBox(
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

ListBox* ListBehavior::ResolveListBox(
    std::uint32_t index) noexcept {
    ::Aero::Media::Visual* visual =
        tree_->ResolveHandle(records_[index].handle);
    return visual != nullptr
        ? static_cast<ListBox*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void> ListBehavior::Attach(
    ListBox& listBox) noexcept {
    if (listBox.GetTree() != tree_ ||
        FindListBox(listBox) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ListBox interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(listBox);
    if (!handle) return handle.GetStatus();
    if (records_.Empty()) {
        input_->AddPointerStateChanged(pointerStateChangedHandler_);
    }
    Base::Result<void> mouse =
        listBox.AddHandlerChecked(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        listBox.AddHandlerChecked(
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
        records_.PushBack(record);
    if (!added) {
        static_cast<void>(listBox.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        static_cast<void>(listBox.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        return added.GetStatus();
    }
    ::Aero::Core::InteractionStateFacet::SyncSelectorContainers(listBox);
    return {};
}

Base::Result<bool> ListBehavior::Detach(
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
    return true;
}

std::uint32_t
ListBehavior::FindContainerIndex(
    ListBox& listBox,
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !listBox.PropertyRegistry().Types()
            .IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    ::Aero::Media::Visual* visual =
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
                listBox.GetItemContainerGenerator();
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
ListBehavior::ApplyUserSelection(
    ListBox& listBox,
    Record& record,
    std::uint32_t index,
    std::uint32_t modifiers) noexcept {
    const SelectionMode mode =
        listBox.GetSelectionMode();
    if (mode == SelectionMode::Single) {
        record.anchorIndex = index;
        listBox.SetSelectedIndex(index);
        if (!listBox.LastSelectionError().IsOk()) {
            return listBox.LastSelectionError();
        }
        return true;
    }
    if (mode == SelectionMode::Multiple) {
        record.anchorIndex = index;
        const bool changed = listBox.Toggle(index);
        return listBox.LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(listBox.LastSelectionError());
    }
    const bool shift = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Control);
    if (shift) {
        if (record.anchorIndex == UINT32_MAX ||
            record.anchorIndex >=
                listBox.GetCount()) {
            record.anchorIndex =
                listBox.GetSelectedIndex() != UINT32_MAX
                ? listBox.GetSelectedIndex()
                : index;
        }
        const bool changed = listBox.SelectRange(
                record.anchorIndex,
                index,
                control);
        return listBox.LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(listBox.LastSelectionError());
    }
    record.anchorIndex = index;
    if (control) {
        const bool changed = listBox.Toggle(index);
        return listBox.LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(listBox.LastSelectionError());
    }
    listBox.SetSelectedIndex(index);
    if (!listBox.LastSelectionError().IsOk()) {
        return listBox.LastSelectionError();
    }
    return true;
}

void ListBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    if (args.GetChangedButton() != MouseButton::Left) {
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
            listBox, args.GetOriginalSource());
    if (index == UINT32_MAX) return;
    Base::Result<bool> selected =
        ApplyUserSelection(
            listBox,
            records_[recordIndex],
            index,
            args.GetModifiers());
    if (!selected) return;
    ItemContainerGenerator* generator =
        listBox.GetItemContainerGenerator();
    if (generator != nullptr) {
        static_cast<void>(input_->SetFocus(
            generator->ContainerFromIndex(index)));
    }
    static_cast<void>(
        listBox.BringIntoView(index));
    args.SetHandled(true);
}

void ListBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown &&
        args.GetKey() != KeyboardKeyHome &&
        args.GetKey() != KeyboardKeyEnd) {
        return;
    }
    auto& listBox =
        *static_cast<ListBox*>(sender);
    if (!listBox.GetIsEnabled() ||
        listBox.GetCount() == 0U) {
        return;
    }
    const std::uint32_t recordIndex =
        FindListBox(listBox);
    if (recordIndex == UINT32_MAX) return;
    std::uint32_t current =
        FindContainerIndex(
            listBox, args.GetOriginalSource());
    if (current == UINT32_MAX) {
        current =
            listBox.GetSelectedIndex() != UINT32_MAX
            ? listBox.GetSelectedIndex()
            : 0U;
    }
    std::uint32_t target = current;
    if (args.GetKey() == KeyboardKeyUp &&
        target > 0U) {
        --target;
    } else if (args.GetKey() == KeyboardKeyDown &&
        target + 1U < listBox.GetCount()) {
        ++target;
    } else if (args.GetKey() == KeyboardKeyHome) {
        target = 0U;
    } else if (args.GetKey() == KeyboardKeyEnd) {
        target = listBox.GetCount() - 1U;
    }
    const bool control = HasKeyboardModifier(
        args.GetModifiers(),
        KeyboardModifiers::Control);
    const bool shift = HasKeyboardModifier(
        args.GetModifiers(),
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
                args.GetModifiers());
        if (!selected) return;
    }
    ItemContainerGenerator* generator =
        listBox.GetItemContainerGenerator();
    if (generator != nullptr) {
        static_cast<void>(input_->SetFocus(
            generator->ContainerFromIndex(target)));
    }
    static_cast<void>(
        listBox.BringIntoView(target));
    args.SetHandled(true);
}

void ListBehavior::OnPointerStateChanged(
    UIElement& element) noexcept {
    if (states_ == nullptr) return;
    for (std::uint32_t i = 0U; i < records_.Size(); ++i) {
        ListBox* listBox = ResolveListBox(i);
        if (listBox == nullptr) continue;
        const std::uint32_t index =
            FindContainerIndex(*listBox, &element);
        if (index != UINT32_MAX) {
            ItemContainerGenerator* generator =
                listBox->GetItemContainerGenerator();
            if (generator != nullptr) {
                FrameworkElement* container =
                    generator->ContainerFromIndex(index);
                if (container != nullptr &&
                    listBox->PropertyRegistry().Types().IsDerivedFrom(
                        container->RuntimeType(),
                        ListBoxItem::StaticTypeId())) {
                    auto& item =
                        *static_cast<ListBoxItem*>(container);
                    Base::StringView common = "Normal";
                    if (!item.GetIsEnabled()) {
                        common = "Disabled";
                    } else if (item.GetIsMouseOver()) {
                        common = "MouseOver";
                    }
                    static_cast<void>(
                        Aero::Controls::TemplatePrivate::GoToState(
                            *states_,
                            item,
                            "CommonStates",
                            common,
                            true));
                    const bool selected = item.GetIsSelected();
                    static_cast<void>(
                        Aero::Controls::TemplatePrivate::GoToState(
                            *states_,
                            item,
                            "SelectionStates",
                            selected
                                ? Base::StringView("Selected")
                                : Base::StringView("Unselected"),
                            true));
                }
            }
            return;
        }
    }
}

} // namespace Aero::Controls

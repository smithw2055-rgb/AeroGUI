#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/Data/CollectionViewSource.hpp>


#include <algorithm>
#include <utility>

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
    values.Reserve(values.Size() + 1U);
    values.PushBack(value);
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

ListBoxItem::ListBoxItem() noexcept
    : ListBoxItem(StaticTypeId()) {}

ListBoxItem::ListBoxItem(TypeId runtimeType) noexcept
    : ContentControl(runtimeType),
      selectedChangedHandler_(
          this, &ListBoxItem::OnIsSelectedChanged),
      mouseOverChangedHandler_(
          this, &ListBoxItem::OnIsMouseOverChanged),
      isEnabledChangedHandler_(
          this, &ListBoxItem::OnIsEnabledChanged) {
    static_cast<void>(AddValueChangedHandler(
        IsSelectedProperty, selectedChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        UIElement::IsMouseOverProperty, mouseOverChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        UIElement::IsEnabledProperty, isEnabledChangedHandler_));
}

ListBoxItem::~ListBoxItem() {
    static_cast<void>(RemoveValueChangedHandler(
        IsSelectedProperty, selectedChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        UIElement::IsMouseOverProperty, mouseOverChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        UIElement::IsEnabledProperty, isEnabledChangedHandler_));
}

bool ListBoxItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void ListBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

void ListBoxItem::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView common = "Normal";
    if (!GetIsEnabled()) {
        common = "Disabled";
    } else if (GetIsMouseOver()) {
        common = "MouseOver";
    }
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            common,
            useTransitions));
    const bool selected = GetIsSelected();
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            selected
                ? Base::StringView("Selected")
                : Base::StringView("Unselected"),
            useTransitions));
}

void ListBoxItem::OnIsSelectedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    UpdateVisualState(true);
    const bool selected =
        args.GetNewValue().Kind() == Meta::ValueKind::Boolean &&
        args.GetNewValue().AsBoolean();
    if (!selected) return;
    ::Aero::Media::Visual* visual = this;
    while (visual != nullptr) {
        UIElement* element = ::Aero::TryCast<UIElement>(visual);
        if (element != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
                element->RuntimeType(), ListBox::StaticTypeId())) {
            auto& listBox = *static_cast<ListBox*>(element);
            ItemContainerGenerator* generator =
                listBox.GetItemContainerGenerator();
            if (generator == nullptr) return;
            const std::uint32_t index =
                generator->IndexFromContainer(*this);
            if (index != UINT32_MAX &&
                listBox.GetSelectedIndex() != index) {
                listBox.SetSelectedIndex(index);
            }
            return;
        }
        visual = visual->GetVisualParent();
    }
}

void ListBoxItem::OnIsMouseOverChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    UpdateVisualState(true);
    if (GetIsMouseOver() && GetIsEnabled()) {
        ::Aero::Media::Visual* visual = this;
        while (visual != nullptr) {
            UIElement* element = ::Aero::TryCast<UIElement>(visual);
            if (element != nullptr &&
                AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
                    element->RuntimeType(), ListBox::StaticTypeId())) {
                auto& listBox = *static_cast<ListBox*>(element);
                if (listBox.GetSelectionMode() == SelectionMode::Single) {
                    ItemContainerGenerator* generator =
                        listBox.GetItemContainerGenerator();
                    if (generator != nullptr) {
                        const std::uint32_t index =
                            generator->IndexFromContainer(*this);
                        if (index != UINT32_MAX &&
                            listBox.GetSelectedIndex() != index) {
                            listBox.SetSelectedIndex(index);
                        }
                    }
                }
                break;
            }
            visual = visual->GetVisualParent();
        }
    }
}

void ListBoxItem::OnIsEnabledChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    UpdateVisualState(true);
}

Selector::Selector() noexcept
    : Selector(StaticTypeId()) {}

Selector::Selector(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType),
      itemsChangedHandler_(
          this, &Selector::OnItemsChanged),
      propertyChangedHandler_(
          this, &Selector::OnPropertyChanged),
      currentChangedHandler_(
          this, &Selector::OnViewCurrentChanged) {
    AddItemsChanged(itemsChangedHandler_);
    static_cast<void>(AddValueChangedHandler(
        SelectionModeProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        SelectedIndexProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        SelectedItemProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        SelectedValueProperty,
        propertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        IsSynchronizedWithCurrentItemProperty,
        propertyChangedHandler_));
}

Selector::~Selector() {
    UnhookCurrentView();
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
    static_cast<void>(RemoveValueChangedHandler(
        IsSynchronizedWithCurrentItemProperty,
        propertyChangedHandler_));
}

SelectionMode Selector::GetSelectionMode() const noexcept {
    return GetValue(SelectionModeProperty);
}

std::uint32_t Selector::GetSelectedIndex() const noexcept {
    return GetValue(SelectedIndexProperty);
}

Base::Ref<Base::Object>
Selector::GetSelectedItem() const noexcept {
    return GetValue(SelectedItemProperty);
}

Base::Ref<Base::Object>
Selector::GetSelectedValue() const noexcept {
    return GetValue(SelectedValueProperty);
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

bool Selector::GetIsSynchronizedWithCurrentItem() const noexcept {
    return GetValue(IsSynchronizedWithCurrentItemProperty);
}

void Selector::SetIsSynchronizedWithCurrentItem(bool value) noexcept {
    SetValue(IsSynchronizedWithCurrentItemProperty, value);
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
    selection.Reserve(
            selectedIndices_.Size() + 1U);
    for (std::uint32_t selected :
        selectedIndices_) {
        selection.PushBack(selected);
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
    selection.Reserve(
            selectedIndices_.Size() - 1U);
    for (std::uint32_t selected :
        selectedIndices_) {
        if (selected == index) continue;
        selection.PushBack(selected);
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
    selection.Reserve(
            (preserveExisting
                ? selectedIndices_.Size()
                : 0U) +
            (end - begin + 1U));
    if (preserveExisting) {
        for (std::uint32_t selected :
            selectedIndices_) {
            selection.PushBack(selected);
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
    normalized.Reserve(indices.Size());
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
        normalized.PushBack(selected);
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
    removed.Reserve(selectedIndices_.Size());
    added.Reserve(normalized.Size());
    for (std::uint32_t index : selectedIndices_) {
        if (!ContainsIndex(newSelection, index)) {
            removed.PushBack(index);
        }
    }
    for (std::uint32_t index : normalized) {
        if (!ContainsIndex(oldSelection, index)) {
            added.PushBack(index);
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
    PushSelectionToCurrent();
    return {};
}

void Selector::SyncContainers() noexcept {
    auto* states = static_cast<Aero::VisualStateManager*>(
        AeroGuiInternal::VisualStateRuntime(*this));
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    // ContainerFromIndex takes an item index. Virtualization may start at
    // firstGeneratedIndex_ > 0; looping generated slots as item indices
    // misses realized containers.
    const std::uint32_t firstGeneratedIndex =
        generator->GetFirstGeneratedIndex();
    const std::uint32_t generatedCount =
        generator->GetGeneratedCount();
    for (std::uint32_t slot = 0U; slot < generatedCount; ++slot) {
        const std::uint32_t index = firstGeneratedIndex + slot;
        FrameworkElement* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
                Aero::Controls::FrameworkTemplateState::GoToState(*states,
                    item,
                    "SelectionStates",
                    selected
                        ? Base::StringView("Selected")
                        : Base::StringView("Unselected")));
        }
    }
}

void Selector::HookCurrentView() noexcept {
    UnhookCurrentView();
    Data::CollectionView* view =
        Data::CollectionViewSource::GetDefaultView(GetItemsSourceCore());
    if (view == nullptr) return;
    view->AddCurrentChanged(currentChangedHandler_);
    subscribedView_ = view;
    if (GetSelectedItem()) {
        PushSelectionToCurrent();
        return;
    }
    Base::Ref<Base::Object> current = view->GetCurrentItem();
    if (!current) return;
    synchronizingCurrent_ = true;
    SetSelectedItem(current);
    synchronizingCurrent_ = false;
}

void Selector::UnhookCurrentView() noexcept {
    if (subscribedView_ == nullptr) return;
    static_cast<void>(
        subscribedView_->RemoveCurrentChanged(currentChangedHandler_));
    subscribedView_ = nullptr;
}

void Selector::PushSelectionToCurrent() noexcept {
    if (synchronizingCurrent_ ||
        !GetIsSynchronizedWithCurrentItem()) {
        return;
    }
    Data::CollectionView* view = subscribedView_;
    if (view == nullptr) {
        view = Data::CollectionViewSource::GetDefaultView(
            GetItemsSourceCore());
    }
    if (view == nullptr) return;
    synchronizingCurrent_ = true;
    static_cast<void>(view->MoveCurrentTo(GetSelectedItem().Get()));
    synchronizingCurrent_ = false;
}

void Selector::OnViewCurrentChanged() noexcept {
    if (synchronizingCurrent_ ||
        !GetIsSynchronizedWithCurrentItem() ||
        subscribedView_ == nullptr) {
        return;
    }
    synchronizingCurrent_ = true;
    SetSelectedItem(subscribedView_->GetCurrentItem());
    synchronizingCurrent_ = false;
}

void Selector::OnItemsSourceCoreChanged() noexcept {
    UnhookCurrentView();
    if (GetIsSynchronizedWithCurrentItem()) {
        HookCurrentView();
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
    mapped.Reserve(
            selectedIndices_.Size());
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
    } else if (args.GetProperty() ==
        IsSynchronizedWithCurrentItemProperty) {
        UnhookCurrentView();
        if (GetIsSynchronizedWithCurrentItem()) {
            HookCurrentView();
        }
        applied = true;
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
    if (AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    if (AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        static_cast<ListBoxItem&>(container).SetIsSelected(false);
    }
    ItemsControl::ClearContainer(container);
}

void Selector::OnContainersChanged() noexcept {
    ItemsControl::OnContainersChanged();
    SyncContainers();
}

ListBox::ListBox() noexcept
    : ListBox(StaticTypeId()) {}

ListBox::ListBox(TypeId runtimeType) noexcept
    : Selector(runtimeType),
      mouseDownHandler_(this, &ListBox::HandleMouseDown),
      keyDownHandler_(this, &ListBox::HandleKeyDown) {
    AddHandler(UIElement::MouseDownEvent, mouseDownHandler_);
    AddHandler(UIElement::KeyDownEvent, keyDownHandler_);
    AeroGuiInternal::SyncSelectorContainers(*this);
}

ListBox::~ListBox() {
    static_cast<void>(RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
}

void ListBox::HandleMouseDown(Base::Object*, MouseButtonEventArgs& args) noexcept {
    OnMouseLeftButtonDown(args);
}

void ListBox::HandleKeyDown(Base::Object*, KeyEventArgs& args) noexcept {
    OnKeyDown(args);
}

std::uint32_t ListBox::FindContainerIndex(Base::Object* source) const noexcept {
    if (source == nullptr ||
        !AeroGuiInternal::PropertyRegistry(*this).Types()
            .IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    ::Aero::Media::Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != this) {
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types()
                .IsDerivedFrom(
                    element->RuntimeType(),
                    ListBoxItem::StaticTypeId())) {
            ItemContainerGenerator* generator =
                GetItemContainerGenerator();
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

Base::Result<bool> ListBox::ApplyUserSelection(
    std::uint32_t index,
    std::uint32_t modifiers) noexcept {
    const SelectionMode mode = GetSelectionMode();
    if (mode == SelectionMode::Single) {
        anchorIndex_ = index;
        SetSelectedIndex(index);
        if (!LastSelectionError().IsOk()) {
            return LastSelectionError();
        }
        return true;
    }
    if (mode == SelectionMode::Multiple) {
        anchorIndex_ = index;
        const bool changed = Toggle(index);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    const bool shift = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Control);
    if (shift) {
        if (anchorIndex_ == UINT32_MAX ||
            anchorIndex_ >= GetCount()) {
            anchorIndex_ =
                GetSelectedIndex() != UINT32_MAX
                ? GetSelectedIndex()
                : index;
        }
        const bool changed = SelectRange(
                anchorIndex_,
                index,
                control);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    anchorIndex_ = index;
    if (control) {
        const bool changed = Toggle(index);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    SetSelectedIndex(index);
    if (!LastSelectionError().IsOk()) {
        return LastSelectionError();
    }
    return true;
}

void ListBox::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    if (!GetIsEnabled()) return;
    const std::uint32_t index = FindContainerIndex(args.GetOriginalSource());
    if (index == UINT32_MAX) return;
    Base::Result<bool> selected = ApplyUserSelection(index, args.GetModifiers());
    if (!selected) return;
    ItemContainerGenerator* generator = GetItemContainerGenerator();
    if (generator != nullptr) {
        FrameworkElement* container = generator->ContainerFromIndex(index);
        if (container != nullptr) {
            static_cast<void>(container->Focus());
        }
    }
    static_cast<void>(BringIntoView(index));
    args.SetHandled(true);
}

void ListBox::OnKeyDown(KeyEventArgs& args) {
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown &&
        args.GetKey() != KeyboardKeyHome &&
        args.GetKey() != KeyboardKeyEnd) {
        return;
    }
    if (!GetIsEnabled() || GetCount() == 0U) {
        return;
    }
    std::uint32_t current = FindContainerIndex(args.GetOriginalSource());
    if (current == UINT32_MAX) {
        current =
            GetSelectedIndex() != UINT32_MAX
            ? GetSelectedIndex()
            : 0U;
    }
    std::uint32_t target = current;
    if (args.GetKey() == KeyboardKeyUp && target > 0U) {
        --target;
    } else if (args.GetKey() == KeyboardKeyDown && target + 1U < GetCount()) {
        ++target;
    } else if (args.GetKey() == KeyboardKeyHome) {
        target = 0U;
    } else if (args.GetKey() == KeyboardKeyEnd) {
        target = GetCount() - 1U;
    }
    const bool control = HasKeyboardModifier(
        args.GetModifiers(), KeyboardModifiers::Control);
    const bool shift = HasKeyboardModifier(
        args.GetModifiers(), KeyboardModifiers::Shift);
    if (!control || GetSelectionMode() != SelectionMode::Extended || shift) {
        Base::Result<bool> selected = ApplyUserSelection(target, args.GetModifiers());
        if (!selected) return;
    }
    ItemContainerGenerator* generator = GetItemContainerGenerator();
    if (generator != nullptr) {
        FrameworkElement* container = generator->ContainerFromIndex(target);
        if (container != nullptr) {
            static_cast<void>(container->Focus());
        }
    }
    static_cast<void>(BringIntoView(target));
    args.SetHandled(true);
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
    if (generator == nullptr) {
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
            ::Aero::TryCast<::Aero::UIElement>(parent);
        if (parentElement != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    return GetValue(IsSelectedProperty);
}

void ComboBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

ComboBox::ComboBox() noexcept
    : Selector(StaticTypeId()),
      mouseDownHandler_(
          this,
          &ComboBox::HandleMouseDown),
      keyDownHandler_(
          this,
          &ComboBox::HandleKeyDown),
      mouseOverChangedHandler_(
          this,
          &ComboBox::OnIsMouseOverChanged),
      isEnabledChangedHandler_(
          this,
          &ComboBox::OnIsEnabledChanged),
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
    AddHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_,
        true);
    AddHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_);
    static_cast<void>(AddValueChangedHandler(
        UIElement::IsMouseOverProperty,
        mouseOverChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        UIElement::IsEnabledProperty,
        isEnabledChangedHandler_));
    static_cast<void>(AddSelectionChanged(
        selectionChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        IsDropDownOpenProperty,
        dropDownChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MaxDropDownHeightProperty,
        maxDropDownHeightChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        IsEditableProperty,
        editableChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        TextProperty,
        textChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        Selector::SelectedIndexProperty,
        selectedValueChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        Selector::SelectedItemProperty,
        selectedValueChangedHandler_));
}

ComboBox::~ComboBox() {
    ObserveSelectedProjection(nullptr);
    static_cast<void>(RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        UIElement::IsMouseOverProperty,
        mouseOverChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        UIElement::IsEnabledProperty,
        isEnabledChangedHandler_));
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
    return GetValue(IsDropDownOpenProperty);
}

void ComboBox::SetIsDropDownOpen(
    bool value) noexcept {
    SetValue(IsDropDownOpenProperty, value);
}

double ComboBox::GetMaxDropDownHeight() const noexcept {
    return GetValue(MaxDropDownHeightProperty);
}

void ComboBox::SetMaxDropDownHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(MaxDropDownHeightProperty, value);
}

bool ComboBox::GetIsEditable() const noexcept {
    return GetValue(IsEditableProperty);
}

bool ComboBox::GetIsReadOnly() const noexcept {
    return GetValue(IsReadOnlyProperty);
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
    return GetValue(TextProperty);
}

void ComboBox::SetText(
    Base::StringView value) noexcept {
    SetValue(TextProperty, value);
}

Base::StringView ComboBox::GetSelectionBoxText() const noexcept {
    return GetValue(SelectionBoxTextProperty);
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
    if (AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    if (AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
            !AeroGuiInternal::PropertyRegistry(*this).Types().
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
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                selection->RuntimeType(),
                TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(selection)
        : nullptr;
    DependencyObject* contentSite =
        GetTemplateChild("ContentSite");
    selectionPresenter_ =
        contentSite != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                contentSite->RuntimeType(),
                ContentPresenter::StaticTypeId())
        ? static_cast<ContentPresenter*>(
              contentSite)
        : nullptr;
    if (selectionBox_ == nullptr &&
        selectionPresenter_ != nullptr &&
        selectionPresenter_->GetContent() != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().
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
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                editable->RuntimeType(),
                TextBox::StaticTypeId())
        ? static_cast<TextBox*>(editable)
        : nullptr;
    DependencyObject* border =
        GetTemplateChild("DropDownBorder");
    dropDownBorder_ =
        border != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                border->RuntimeType(),
                FrameworkElement::StaticTypeId())
        ? static_cast<FrameworkElement*>(border)
        : nullptr;
    DependencyObject* popup =
        GetTemplateChild("PART_Popup");
    popup_ =
        popup != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                popup->RuntimeType(),
                Popup::StaticTypeId())
        ? static_cast<Popup*>(popup)
        : nullptr;
    if (editableTextBox_ != nullptr) {
        editableTextBox_->AddHandler(
            TextBox::TextChangedEvent,
            editableTextChangedHandler_);
    }
    if (popup_ != nullptr) {
        static_cast<void>(popup_->AddValueChangedHandler(
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
        selectedProjection_->AddValueChangedHandler(
            TextBlock::TextProperty,
            selectedProjectionChangedHandler_);
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
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                TextBlock::StaticTypeId())) {
        text = static_cast<TextBlock*>(
            selected.Get())->GetText();
    } else if (
        selected &&
        AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                ContentControl::StaticTypeId())) {
        UIElement* content =
            AeroGuiInternal::ContentControlContent(*static_cast<ContentControl*>(
                selected.Get()));
        if (content != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types().
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
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
                container->RuntimeType(),
                ContentControl::StaticTypeId())) {
            UIElement* content = AeroGuiInternal::ContentControlContent(
                *static_cast<ContentControl*>(container));
            if (content != nullptr &&
                AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
        !AeroGuiInternal::PropertyRegistry(*this).Types().
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
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types().
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

void ComboBox::HandleMouseDown(Base::Object*, MouseButtonEventArgs& args) noexcept {
    OnMouseLeftButtonDown(args);
}

void ComboBox::HandleKeyDown(Base::Object*, KeyEventArgs& args) noexcept {
    OnKeyDown(args);
}

void ComboBox::OnIsMouseOverChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    UpdateVisualState(true);
}

void ComboBox::OnIsEnabledChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    UpdateVisualState(true);
}

void ComboBox::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView comboCommon = "Normal";
    if (!GetIsEnabled()) {
        comboCommon = "Disabled";
    } else if (GetIsMouseOver()) {
        comboCommon = "MouseOver";
    }
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            comboCommon,
            useTransitions));
}

void ComboBox::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    if (!GetIsEnabled()) return;
    const std::uint32_t index = FindContainerIndex(args.GetOriginalSource());
    if (index != UINT32_MAX) {
        SetSelectedIndex(index);
        SetIsDropDownOpen(false);
    } else {
        SetIsDropDownOpen(!GetIsDropDownOpen());
    }
    static_cast<void>(Focus());
    args.SetHandled(true);
}

void ComboBox::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (args.GetKey() == KeyboardKeyEscape) {
        if (!GetIsDropDownOpen()) return;
        SetIsDropDownOpen(false);
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyEnter ||
        args.GetKey() == KeyboardKeySpace) {
        SetIsDropDownOpen(!GetIsDropDownOpen());
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown) {
        return;
    }
    if (GetCount() == 0U) return;
    std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) {
        selected = 0U;
    } else if (
        args.GetKey() == KeyboardKeyDown &&
        selected + 1U < GetCount()) {
        ++selected;
    } else if (
        args.GetKey() == KeyboardKeyUp &&
        selected > 0U) {
        --selected;
    }
    SetSelectedIndex(selected);
    args.SetHandled(true);
}

} // namespace Aero::Controls

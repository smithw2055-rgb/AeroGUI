#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Data/CollectionView.hpp>


#include <Aero/Base/String.hpp>
#include "gui/core/ValueConversion.hpp"
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

Selector::Selector() noexcept
    : Selector(StaticTypeId()) {}

Selector::Selector(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType),
      itemsChangedHandler_(
          this, &Selector::OnItemsChanged),
      currentChangedHandler_(
          this, &Selector::OnViewCurrentChanged) {
    AddItemsChanged(itemsChangedHandler_);
}

Selector::~Selector() {
    UnhookCurrentView();
    static_cast<void>(
        RemoveItemsChanged(itemsChangedHandler_));
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
    PublishProperties();
    SyncContainers();
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
    OnSelectionChanged(event);
    if (!selectionChanged_.Empty()) {
        selectionChanged_.Invoke(*this, event);
    }
    RoutedEventArgs routedArgs;
    RaiseEvent(
        SelectionChangedRoutedEvent,
        &routedArgs);
    lastSelectionError_ = {};
    return true;
}

void Selector::PublishProperties() noexcept {
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
}

void Selector::SyncContainers() noexcept {
    auto* states = static_cast<Aero::VisualStateManager*>(
        ElementTree::VisualStatesOf(*this));
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
            !(*this).PropertyRegistry().Types().IsDerivedFrom(
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

PropertyValue Selector::CoerceValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& baseValue) noexcept {
    if (property != SelectedItemProperty.Handle() &&
        property != SelectedValueProperty.Handle()) {
        return baseValue;
    }
    if (baseValue.Kind() != Meta::ValueKind::Object ||
        baseValue.IsNullObject()) {
        return baseValue;
    }
    const Base::Ref<Base::Object> item = baseValue.AsObject();
    if (item && GetCount() != 0U &&
        GetIndexOfItem(item.Get()) == UINT32_MAX) {
        return PropertyValue::NullObject(Meta::TypeOf<Base::Object>());
    }
    return baseValue;
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
            PublishProperties();
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

void Selector::OnSelectionChanged(const SelectionChangedEvent&) {}

void Selector::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ItemsControl::OnPropertyChanged(args);
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
            PublishProperties();
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

Base::Result<void> Selector::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Base::Result<void> prepared =
        ItemsControl::PrepareContainerForItemOverride(
            container, item, index);
    if (!prepared) return prepared.GetStatus();
    if ((*this).PropertyRegistry().Types().IsDerivedFrom(
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

void Selector::ClearContainerForItemOverride(
    FrameworkElement& container) noexcept {
    if ((*this).PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        static_cast<ListBoxItem&>(container).SetIsSelected(false);
    }
    ItemsControl::ClearContainerForItemOverride(container);
}

void Selector::OnContainersChanged() noexcept {
    ItemsControl::OnContainersChanged();
    SyncContainers();
}

namespace Primitives {

AERO_DESCRIBE(Selector) {
    using namespace Aero::Meta;
    Register<Selector>(context, TypeFlags::Abstract)
        .Event(Selector::SelectionChangedRoutedEvent)
        .Property(Selector::SelectionModeProperty, SelectionMode::Single)
        .Property(Selector::SelectedIndexProperty, UINT32_MAX, BindsTwoWayByDefault)
        .Property(Selector::SelectedItemProperty, FrameworkPropertyMetadata(Base::Ref<Base::Object>{}, BindsTwoWayByDefault))
        .Property(Selector::SelectedValueProperty, FrameworkPropertyMetadata(Base::Ref<Base::Object>{}, BindsTwoWayByDefault))
        .Property(Selector::SelectedValuePathProperty, Base::String{})
        .Property(Selector::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Property(Selector::IsSynchronizedWithCurrentItemProperty, false);
}

} // namespace Primitives

} // namespace Aero::Controls

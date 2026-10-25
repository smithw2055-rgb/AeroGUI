#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/media/BrushRendering.hpp"
#include "render/DisplayList.hpp"
#include "ControlPropertyValidators.hpp"

#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Decorators.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Documents.hpp>
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <new>
#include <utility>

namespace Aero::Controls {

using namespace Primitives;

// ============================================================================
// Selector
// ============================================================================


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
        ApplySelection(values, selected);
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
    ApplySelection(values, index);
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
        return ApplySelection(values, index);
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
    return ApplySelection(
        {selection.Data(), selection.Size()},
        index);
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
    return ApplySelection(
        {selection.Data(), selection.Size()},
        primary);
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
        return ApplySelection(values, last);
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
    return ApplySelection(
        {selection.Data(), selection.Size()},
        last);
}

void Selector::ClearSelection() noexcept {
    pendingSelectedItem_.Reset();
    ApplySelection({}, UINT32_MAX);
}

bool Selector::ApplySelection(
    Base::Span<const std::uint32_t> indices,
    std::uint32_t primaryIndex) noexcept {
    Base::Vector<std::uint32_t> normalized;
    normalized.Reserve(indices.Size());
    for (std::uint32_t index : indices) {
        if (index >= GetCount()) {
            lastSelectionError_ = Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Selector selection contains an invalid index");
            return false;
        }
        Base::Result<void> inserted =
            InsertSortedUnique(normalized, index);
        if (!inserted) {
            lastSelectionError_ = inserted.GetStatus();
            return false;
        }
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
        lastSelectionError_ = Base::Status::Ok();
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
            !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
            ApplySelection(values, index);
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
            ApplySelection(values, selected);
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
    ApplySelection(
        {mapped.Data(), mapped.Size()},
        mappedPrimary);
}

void Selector::OnSelectionChanged(const SelectionChangedEvent&) {}

void Selector::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ItemsControl::OnPropertyChanged(args);
    if (synchronizingProperties_) return;
    activeProperty_ = args.GetProperty();
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
            ApplySelection(
                values, selected);
        }
    } else if (args.GetProperty() ==
        SelectedIndexProperty) {
        const std::uint32_t index =
            GetSelectedIndex();
        if (index == UINT32_MAX) {
            ClearSelection();
        } else if (index >= GetCount()) {
            pendingIndex_ = index;
            selectedIndices_.Clear();
            primaryIndex_ = UINT32_MAX;
            PublishProperties();
            SyncContainers();
        } else {
            const std::uint32_t values[] = {
                index};
            ApplySelection(
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
        } else {
            const std::uint32_t index =
                GetIndexOfItem(item.Get());
            if (index == UINT32_MAX) {
                pendingSelectedItem_ = item;
                selectedIndices_.Clear();
                primaryIndex_ = UINT32_MAX;
            } else {
                pendingSelectedItem_.Reset();
                const std::uint32_t values[] = {
                    index};
                ApplySelection(
                    values, index);
            }
        }
    } else if (args.GetProperty() ==
        IsSynchronizedWithCurrentItemProperty) {
        UnhookCurrentView();
        if (GetIsSynchronizedWithCurrentItem()) {
            HookCurrentView();
        }
    }
    activeProperty_ = {};
}

void Selector::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    ItemsControl::PrepareContainerForItemOverride(
        container, item, index);
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(),
            ListBoxItem::StaticTypeId())) {
        auto& listBoxItem =
            static_cast<ListBoxItem&>(container);
        listBoxItem.SetIsSelected(GetIsSelected(index));
        listBoxItem.SetIsTabStop(true);
        return;
    }
}

void Selector::ClearContainerForItemOverride(
    FrameworkElement& container) noexcept {
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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


// ============================================================================
// ListBox & ListBoxItem & TabPanel
// ============================================================================


using namespace Primitives;

ListBoxItem::ListBoxItem() noexcept
    : ListBoxItem(StaticTypeId()) {}

ListBoxItem::ListBoxItem(TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

ListBoxItem::~ListBoxItem() = default;

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

void ListBoxItem::OnApplyTemplate() noexcept {
    ContentControl::OnApplyTemplate();
    UpdateVisualState(false);
}

void ListBoxItem::OnSelected(RoutedEventArgs& e) {
    RaiseEvent(SelectedEvent, &e);
}

void ListBoxItem::OnUnselected(RoutedEventArgs& e) {
    RaiseEvent(UnselectedEvent, &e);
}

void ListBoxItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == IsSelectedProperty) {
        UpdateVisualState(true);
        const bool selected =
            args.GetNewValue().Kind() == Meta::ValueKind::Boolean &&
            args.GetNewValue().AsBoolean();
        RoutedEventArgs eventArgs;
        if (selected) {
            OnSelected(eventArgs);
        } else {
            OnUnselected(eventArgs);
        }
        if (!selected) return;
        ::Aero::Media::Visual* visual = this;
        while (visual != nullptr) {
            UIElement* element = ::Aero::TryCast<UIElement>(visual);
            if (element != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
    } else if (prop == UIElement::IsMouseOverProperty) {
        UpdateVisualState(true);
        if (GetIsMouseOver() && GetIsEnabled()) {
            ::Aero::Media::Visual* visual = this;
            while (visual != nullptr) {
                UIElement* element = ::Aero::TryCast<UIElement>(visual);
                if (element != nullptr &&
                    DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
    } else if (prop == UIElement::IsEnabledProperty) {
        UpdateVisualState(true);
    }
}


ListBox::ListBox() noexcept
    : ListBox(StaticTypeId()) {}

ListBox::ListBox(TypeId runtimeType) noexcept
    : Selector(runtimeType) {
    (*this).SyncContainers();
}

ListBox::~ListBox() = default;

std::uint32_t ListBox::FindContainerIndex(Base::Object* source) const noexcept {
    if (source == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types()
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
            DependencyObjectAccess::PropertyRegistry((*this)).Types()
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

bool ListBox::ApplyUserSelection(
    std::uint32_t index,
    std::uint32_t modifiers) noexcept {
    const SelectionMode mode = GetSelectionMode();
    if (mode == SelectionMode::Single) {
        anchorIndex_ = index;
        SetSelectedIndex(index);
        return LastSelectionError().IsOk();
    }
    if (mode == SelectionMode::Multiple) {
        anchorIndex_ = index;
        return Toggle(index) && LastSelectionError().IsOk();
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
        return SelectRange(anchorIndex_, index, control) && LastSelectionError().IsOk();
    }
    anchorIndex_ = index;
    if (control) {
        return Toggle(index) && LastSelectionError().IsOk();
    }
    SetSelectedIndex(index);
    return LastSelectionError().IsOk();
}

void ListBox::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    if (!GetIsEnabled()) return;
    const std::uint32_t index = FindContainerIndex(args.GetOriginalSource());
    if (index == UINT32_MAX) return;
    if (!ApplyUserSelection(index, args.GetModifiers())) return;
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
        if (!ApplyUserSelection(target, args.GetModifiers())) return;
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

Base::Ref<FrameworkElement>
ListBox::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<ListBoxItem>();
}

bool ListBox::BringIntoView(
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
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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

AERO_DESCRIBE(ListBox) {
    using namespace Aero::Meta;
    Register<ListBox>(context)
        .Factory();
    AERO_ON(ListBox, &ListBox::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ListBox, &ListBox::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(ListBoxItem) {
    using namespace Aero::Meta;
    Register<ListBoxItem>(context)
        .Property(ListBoxItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
}




// ============================================================================
// ComboBox & ComboBoxItem
// ============================================================================


using namespace Primitives;

ComboBox::ComboBox() noexcept
    : Selector(StaticTypeId()),
      popupIsOpenChangedHandler_(
          this,
          &ComboBox::OnPopupIsOpenChanged),
      selectedProjectionChangedHandler_(
          this,
          &ComboBox::OnSelectedProjectionChanged),
      editableTextChangedHandler_(
          this,
          &ComboBox::OnEditableTextChanged) {}

ComboBox::~ComboBox() {
    ObserveSelectedProjection(nullptr);
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

Base::Ref<FrameworkElement>
ComboBox::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<ComboBoxItem>();
}

void ComboBox::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Selector::PrepareContainerForItemOverride(
        container, item, index);
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        auto& comboItem =
            static_cast<ComboBoxItem&>(
                container);
        comboItem.SetIsSelected(GetIsSelected(index));
        comboItem.SetIsTabStop(true);
    }
}

void ComboBox::ClearContainerForItemOverride(
    FrameworkElement& container) noexcept {
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        static_cast<ComboBoxItem&>(container).SetIsSelected(false);
    }
    Selector::ClearContainerForItemOverride(container);
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
            !DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
    UpdateSelectionBox();
}

void ComboBox::OnApplyTemplate()
    noexcept {
    Selector::OnApplyTemplate();

    DependencyObject* selection =
        GetTemplateChild("SelectionBox");
    selectionBox_ =
        selection != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selection->RuntimeType(),
                TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(selection)
        : nullptr;
    DependencyObject* contentSite =
        GetTemplateChild("ContentSite");
    if (contentSite == nullptr) {
        contentSite = GetTemplateChild("SelectionBoxItemHost");
    }
    selectionPresenter_ =
        contentSite != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                contentSite->RuntimeType(),
                ContentPresenter::StaticTypeId())
        ? static_cast<ContentPresenter*>(
              contentSite)
        : nullptr;
    if (selectionBox_ != nullptr) {
        selectionBox_->SetForeground(GetForeground());
    }
    DependencyObject* editable =
        GetTemplateChild("PART_EditableTextBox");
    editableTextBox_ =
        editable != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                editable->RuntimeType(),
                TextBox::StaticTypeId())
        ? static_cast<TextBox*>(editable)
        : nullptr;
    DependencyObject* border =
        GetTemplateChild("DropDownBorder");
    dropDownBorder_ =
        border != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                border->RuntimeType(),
                FrameworkElement::StaticTypeId())
        ? static_cast<FrameworkElement*>(border)
        : nullptr;
    DependencyObject* popup =
        GetTemplateChild("PART_Popup");
    popup_ =
        popup != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
    UpdateSelectionBox();
    UpdateEditableVisualState();
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
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    UpdateSelectionBox();
    SynchronizeContainers();
    if (GetIsDropDownOpen()) {
        SetIsDropDownOpen(false);
    }
}

void ComboBox::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == IsDropDownOpenProperty) {
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
    } else if (prop == MaxDropDownHeightProperty) {
        if (dropDownBorder_ != nullptr) {
            static_cast<void>(
                dropDownBorder_->SetMaxSize(
                    {1.0e12,
                     GetMaxDropDownHeight()}));
        }
    } else if (prop == IsEditableProperty) {
        UpdateEditableVisualState();
    } else if (prop == TextProperty) {
        if (editableTextBox_ == nullptr ||
            synchronizingEditableText_ ||
            editableTextBox_->GetText() == GetText()) {
            return;
        }
        synchronizingEditableText_ = true;
        editableTextBox_->SetText(GetText());
        synchronizingEditableText_ = false;
    } else if (prop == Control::ForegroundProperty) {
        if (selectionBox_ != nullptr) {
            selectionBox_->SetForeground(GetForeground());
        }
        if (editableTextBox_ != nullptr) {
            editableTextBox_->SetForeground(GetForeground());
        }
    } else if (prop == Selector::SelectedIndexProperty ||
               prop == Selector::SelectedItemProperty ||
               prop == ItemsControl::ItemsSourceProperty ||
               prop == ItemsControl::ItemTemplateProperty ||
               prop == ItemsControl::DisplayMemberPathProperty) {
        UpdateSelectionBox();
    } else if (prop == UIElement::IsMouseOverProperty ||
               prop == UIElement::IsEnabledProperty) {
        UpdateVisualState(true);
    }
}

void ComboBox::OnPopupIsOpenChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const bool open = args.GetNewValue().AsBoolean();
    if (open != GetIsDropDownOpen()) {
        SetIsDropDownOpen(open);
    }
}

void ComboBox::OnSelectedProjectionChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (&object == selectedProjection_) {
        UpdateSelectionBox();
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

void ComboBox::UpdateSelectionBox() noexcept {
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
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                TextBlock::StaticTypeId())) {
        text = static_cast<TextBlock*>(
            selected.Get())->GetText();
    } else if (
        selected &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                ContentControl::StaticTypeId())) {
        UIElement* content =
            (*static_cast<ContentControl*>(
                selected.Get())).GetContentElement();
        if (content != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                container->RuntimeType(),
                ContentControl::StaticTypeId())) {
            UIElement* content = (*static_cast<ContentControl*>(container)).GetContentElement();
            if (content != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
        Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
        Meta::Registry* metadata =
            bindings != nullptr ? bindings->Metadata() : Meta::CurrentObjectFactory().metadata;
        Base::StringView propName = !GetDisplayMemberPath().Empty()
            ? Base::StringView(GetDisplayMemberPath())
            : Base::StringView("Name");
        const Meta::PropertyInfo* prop = metadata != nullptr
            ? metadata->Types().FindProperty(
                selected->RuntimeType(), propName, true)
            : nullptr;
        if (prop != nullptr) {
            Base::Result<Meta::Value> displayed =
                metadata->GetProperty(*selected, prop->Id());
            if (displayed &&
                displayed.Value().Kind() == Meta::ValueKind::String) {
                text = displayed.Value().AsString();
            }
        }
    }
    Base::String value;
    if (!text.Empty()) {
        Base::Result<void> assigned = value.Assign(text);
        if (assigned) {
            SetReadOnlyCurrentValue(SelectionBoxTextProperty, value);
        }
    } else {
        SetReadOnlyCurrentValue(SelectionBoxTextProperty, value);
    }

    Meta::Value selectionItemValue;
    Base::Ref<Base::Object> selectionBoxTemplate;
    if (Base::Ref<DataTemplate> tpl = GetItemTemplate()) {
        selectionBoxTemplate = std::move(tpl);
    }
    if (!selected) {
        selectionItemValue = Meta::Value::NullObject(Meta::TypeOf<Base::Object>());
        selectionBoxTemplate = nullptr;
    } else if (auto* contentControl = ::Aero::TryCast<ContentControl>(selected.Get())) {
        Value ccContent = contentControl->GetContent();
        if (contentControl->GetContentTemplate()) {
            selectionBoxTemplate = contentControl->GetContentTemplate();
        }
        if (ccContent.Kind() == Meta::ValueKind::Object && !ccContent.IsNullObject()) {
            auto* obj = ccContent.AsObject().Get();
            if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    obj->RuntimeType(), UIElement::StaticTypeId())) {
                selectionItemValue = Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), text).Value();
            } else {
                selectionItemValue = ccContent;
            }
        } else if (ccContent.Kind() != Meta::ValueKind::Unset) {
            selectionItemValue = ccContent;
        } else {
            selectionItemValue = Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), text).Value();
        }
    } else if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                   selected->RuntimeType(), UIElement::StaticTypeId())) {
        selectionItemValue = Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), text).Value();
    } else if (selected && selected->RuntimeType() == BoxedItemValue::StaticTypeId()) {
        selectionItemValue = static_cast<const BoxedItemValue&>(*selected).Value();
    } else {
        selectionItemValue = Meta::Value::FromObject(selected->RuntimeType(), selected);
    }

    if (!selectionBoxTemplate) {
        ItemContainerGenerator* generator = AttachedGenerator();
        const std::uint32_t index = GetSelectedIndex();
        FrameworkElement* container =
            generator != nullptr && index != UINT32_MAX
            ? generator->ContainerFromIndex(index)
            : nullptr;
        if (auto* cbi = ::Aero::TryCast<ContentControl>(container)) {
            if (cbi->GetContentTemplate()) {
                selectionBoxTemplate = cbi->GetContentTemplate();
            }
        }
    }

    SetReadOnlyCurrentValue(SelectionBoxItemProperty, selectionItemValue);
    SetReadOnlyCurrentValue(SelectionBoxItemTemplateProperty, selectionBoxTemplate);

    if (selectionPresenter_ != nullptr) {
        selectionPresenter_->SetValue(ContentPresenter::ContentProperty, selectionItemValue);
        selectionPresenter_->SetValue(ContentPresenter::ContentTemplateProperty, selectionBoxTemplate);
        selectionPresenter_->RefreshContentSource();
    }
    if (selectionBox_ != nullptr) {
        selectionBox_->SetText(text);
    }
    if (!text.Empty()) {
        SetCurrentValue(TextProperty, value);
    }
    UpdateEditableVisualState();
}

void ComboBox::UpdateEditableVisualState() noexcept {
    if (selectionBox_ != nullptr) {
        selectionBox_->SetVisibility(GetIsEditable()
            ? Visibility::Collapsed : Visibility::Visible);
    }
    if (editableTextBox_ == nullptr) {
        return;
    }
    editableTextBox_->SetVisibility(GetIsEditable()
        ? Visibility::Visible : Visibility::Collapsed);
    if (editableTextBox_->GetText() == GetText()) {
        return;
    }
    synchronizingEditableText_ = true;
    editableTextBox_->SetText(GetText());
    synchronizingEditableText_ = false;
}

std::uint32_t ComboBox::FindContainerIndex(
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
            DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
    if (args.GetHandled()) return;
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

bool ComboBoxItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void ComboBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

void ComboBoxItem::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetHandled() || args.GetChangedButton() != MouseButton::Left) return;
    SetIsSelected(true);
    ComboBox* comboBox = nullptr;
    DependencyObject* current = this;
    while (current != nullptr) {
        if (auto* cb = TryCast<ComboBox>(current)) {
            comboBox = cb;
            break;
        }
        if (auto* popup = TryCast<Popup>(current)) {
            if (popup->GetTemplatedParent() != nullptr) {
                comboBox = TryCast<ComboBox>(popup->GetTemplatedParent());
            }
            if (comboBox == nullptr && popup->GetPlacementTarget()) {
                comboBox = TryCast<ComboBox>(popup->GetPlacementTarget().Get());
            }
            if (comboBox != nullptr) break;
        }
        if (auto* vis = TryCast<Media::Visual>(current)) {
            current = vis->GetVisualParent();
        } else {
            current = LogicalTreeHelper::GetParent(*current);
        }
    }
    if (comboBox != nullptr) {
        if (ItemContainerGenerator* gen = comboBox->GetItemContainerGenerator()) {
            const std::uint32_t idx = gen->IndexFromContainer(*this);
            if (idx != UINT32_MAX) {
                comboBox->SetSelectedIndex(idx);
            }
        }
        comboBox->SetIsDropDownOpen(false);
        args.SetHandled(true);
    }
}

void ComboBoxItem::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (args.GetHandled() || args.GetChangedButton() != MouseButton::Left) return;
    OnMouseLeftButtonDown(args);
}

AERO_DESCRIBE(ComboBox) {
    using namespace Aero::Meta;
    Register<ComboBox>(context)
        .Event(ComboBox::DropDownOpenedEvent)
        .Event(ComboBox::DropDownClosedEvent)
        .Property(ComboBox::IsDropDownOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(ComboBox::MaxDropDownHeightProperty, 240.0, AffectsMeasure, &Base::Validate::Positive<double>)
        .Property(ComboBox::IsEditableProperty, false, AffectsMeasure | AffectsRender)
        .Property(ComboBox::IsReadOnlyProperty, false, AffectsRender)
        .Property(ComboBox::TextProperty, Base::String{}, AffectsMeasure | BindsTwoWayByDefault)
        .Property(ComboBox::PlaceholderProperty, Base::String{}, AffectsMeasure | AffectsRender)
        .Property(ComboBox::SelectionBoxTextProperty, Base::String{})
        .Property(ComboBox::SelectionBoxItemProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Property(ComboBox::SelectionBoxItemTemplateProperty, Base::Ref<Base::Object>{})
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .TemplatePart("PART_EditableTextBox", TypeOf<TextBox>())
        .Factory();
    AERO_ON(ComboBox, &ComboBox::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ComboBox, &ComboBox::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(ComboBoxItem) {
    using namespace Aero::Meta;
    Register<ComboBoxItem>(context)
        .Property(ComboBoxItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
    AERO_ON(ComboBoxItem, &ComboBoxItem::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ComboBoxItem, &ComboBoxItem::OnMouseLeftButtonUp, UIElement::MouseLeftButtonUpEvent);
}


// ============================================================================
// TabControl & TabItem
// ============================================================================


using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;


bool TabItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void TabItem::SetIsSelected(
    bool value) noexcept {
    SetValue(
        IsSelectedProperty, value);
}

TabControl* TabItem::FindParentTabControl() const noexcept {
    const DependencyObject* current = GetLogicalParent();
    if (current == nullptr) current = GetVisualParent();
    while (current != nullptr) {
        if (const auto* tc = TryCast<TabControl>(current)) {
            return const_cast<TabControl*>(tc);
        }
        if (const auto* fe = TryCast<FrameworkElement>(current)) {
            current = fe->GetLogicalParent() ? fe->GetLogicalParent() : fe->GetTemplatedParent();
        } else if (const auto* vis = TryCast<Media::Visual>(current)) {
            current = vis->GetVisualParent();
        } else {
            break;
        }
    }
    return nullptr;
}

void TabItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == ContentProperty.Handle()) {
        if (GetIsSelected()) {
            if (auto* tc = FindParentTabControl()) {
                tc->SynchronizeSelection();
            }
        }
    } else if (args.GetProperty() == IsSelectedProperty.Handle()) {
        if (args.GetNewValue().Kind() == ValueKind::Boolean && args.GetNewValue().AsBoolean()) {
            if (auto* tc = FindParentTabControl()) {
                const std::uint32_t count = tc->GetCount();
                ItemContainerGenerator* gen = tc->GetItemContainerGenerator();
                for (std::uint32_t i = 0U; i < count; ++i) {
                    if (tc->GetItem(i).Get() == this ||
                        (gen != nullptr && gen->ContainerFromIndex(i) == this)) {
                        if (tc->GetSelectedIndex() != i) {
                            tc->SetSelectedIndex(i);
                        }
                        break;
                    }
                }
            }
        }
    }
}

void TabItem::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    SetIsSelected(true);
    args.SetHandled(true);
}

TabControl::TabControl() noexcept
    : Selector(StaticTypeId()) {}

TabControl::~TabControl() = default;

TabItem* TabControl::GetSelectedTab() const noexcept {
    const std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) return nullptr;
    const Ref<Base::Object> item = GetItem(selected);
    if (item &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            item->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(item.Get());
    }
    ItemContainerGenerator* generator = AttachedGenerator();
    if (generator == nullptr) return nullptr;
    FrameworkElement* container = generator->ContainerFromIndex(selected);
    if (container != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(container);
    }
    return nullptr;
}

Base::Ref<FrameworkElement> TabControl::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<TabItem>();
}

void TabControl::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Selector::PrepareContainerForItemOverride(container, item, index);

    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(), TabItem::StaticTypeId())) {
        auto& tabItem = static_cast<TabItem&>(container);
        if (item && item.Get() != &container) {
            tabItem.SetHeader(Value::FromObject(item->RuntimeType(), item));
            if (Base::Ref<DataTemplate> tpl = GetItemTemplate()) {
                tabItem.SetHeaderTemplate(std::move(tpl));
            }
            tabItem.SetContent(item.Get());
            tabItem.SetValue(ContentControl::ContentProperty,
                Value::FromObject(item->RuntimeType(), item));
        }
        tabItem.SetIsSelected(index == GetSelectedIndex());
    }
}

void TabControl::OnApplyTemplate() noexcept {
    Selector::OnApplyTemplate();
    EnsureDefaultSelection();
}

void TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value = GetSelectedIndex();
    const std::uint32_t count = GetCount();
    ItemContainerGenerator* generator = AttachedGenerator();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        } else if (generator != nullptr) {
            FrameworkElement* container = generator->ContainerFromIndex(index);
            if (container != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    container->RuntimeType(), TabItem::StaticTypeId())) {
                tab = static_cast<TabItem*>(container);
            }
        }
        if (tab != nullptr) {
            tab->SetIsSelected(index == value);
        }
    }
    TabItem* selected = GetSelectedTab();
    Meta::Value selectedContent =
        selected != nullptr
        ? selected->GetContent()
        : Meta::Value::NullObject(
              Meta::TypeOf<Base::Object>());
    if ((selectedContent.Kind() == Meta::ValueKind::Unset ||
         selectedContent.IsNullObject()) &&
        value < count) {
        const Ref<Base::Object> item = GetItem(value);
        if (item && item.Get() != selected) {
            selectedContent = Value::FromObject(item->RuntimeType(), item);
        }
    }
    SetReadOnlyCurrentValue(SelectedContentProperty, selectedContent);

    Ref<Base::Object> selectedContentTemplate;
    if (selected != nullptr && selected->GetContentTemplate()) {
        selectedContentTemplate = selected->GetContentTemplate();
    } else if (Ref<DataTemplate> ct = GetContentTemplate()) {
        selectedContentTemplate = std::move(ct);
    }
    SetReadOnlyCurrentValue(SelectedContentTemplateProperty, selectedContentTemplate);
    InvalidateMeasure();
}

void TabControl::OnSelectionChanged(
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    SynchronizeSelection();
}

void TabControl::EnsureDefaultSelection() noexcept {
    // WPF/Noesis parity: a TabControl with items but no selection
    // automatically selects the first tab. Without this, SelectedContent
    // stays null and the content host renders an empty grey box
    // (gallery TabControl sample).
    if (GetSelectedIndex() == UINT32_MAX && GetCount() > 0U) {
        SetSelectedIndex(0U);
    } else {
        SynchronizeSelection();
    }
}

void TabControl::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Selector::OnItemsChanged(event);
    static_cast<void>(event);
    EnsureDefaultSelection();
}

void TabControl::OnContainersChanged() noexcept {
    Selector::OnContainersChanged();
    EnsureDefaultSelection();
}

void TabControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    if (args.GetProperty() == SelectedIndexProperty ||
        args.GetProperty() == ContentTemplateProperty) {
        SynchronizeSelection();
    }
}

Size TabControl::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return Control::MeasureOverride(availableSize);
    }
    constexpr double HeaderExtent = 28.0;
    const bool verticalStrip =
        GetTabStripPlacement() == Dock::Left ||
        GetTabStripPlacement() == Dock::Right;
    TabItem* selected = GetSelectedTab();
    if (selected == nullptr) {
        return verticalStrip
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    MeasureChild(
            *selected,
            verticalStrip
                ? Size{std::max(0.0, availableSize.width - HeaderExtent),
                    availableSize.height}
                : Size{availableSize.width,
                    std::max(0.0, availableSize.height - HeaderExtent)});
    const Size desired = selected->GetDesiredSize();
    return verticalStrip
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size TabControl::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return Control::ArrangeOverride(finalSize);
    }
    constexpr double HeaderExtent = 28.0;
    const Dock placement = GetTabStripPlacement();
    const bool verticalStrip =
        placement == Dock::Left || placement == Dock::Right;
    TabItem* selected = GetSelectedTab();
    const std::uint32_t count = GetCount();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        }
        if (tab == nullptr) continue;
        Rect slot{};
        if (tab == selected) {
            if (verticalStrip) {
                slot = {placement == Dock::Left ? HeaderExtent : 0.0,
                    0.0, std::max(0.0, finalSize.width - HeaderExtent),
                    finalSize.height};
            } else {
                slot = {0.0, placement == Dock::Top ? HeaderExtent : 0.0,
                    finalSize.width,
                    std::max(0.0, finalSize.height - HeaderExtent)};
            }
        }
        ArrangeChild(*tab, slot);
    }
    return finalSize;
}

bool TabPanel::GetIsVertical() const noexcept {
    const DependencyObject* parent = GetTemplatedParent();
    return parent != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            parent->RuntimeType(), TabControl::StaticTypeId()) &&
        (static_cast<const TabControl*>(parent)->GetTabStripPlacement() ==
             Dock::Left ||
         static_cast<const TabControl*>(parent)->GetTabStripPlacement() ==
             Dock::Right);
}

Size TabPanel::MeasureOverride(
    Size availableSize) noexcept {
    const bool vertical = GetIsVertical();
    Size desired{};
    double linePrimary = 0.0;
    double lineCross = 0.0;
    const double limit = vertical
        ? availableSize.height
        : availableSize.width;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(*child, availableSize);
        const Size size = child->GetDesiredSize();
        const double primary = vertical ? size.height : size.width;
        const double cross = vertical ? size.width : size.height;
        if (linePrimary > 0.0 && limit < 1.0e11 &&
            linePrimary + primary > limit) {
            if (vertical) {
                desired.width += lineCross;
                desired.height = std::max(desired.height, linePrimary);
            } else {
                desired.width = std::max(desired.width, linePrimary);
                desired.height += lineCross;
            }
            linePrimary = 0.0;
            lineCross = 0.0;
        }
        linePrimary += primary;
        lineCross = std::max(lineCross, cross);
    }
    if (vertical) {
        desired.width += lineCross;
        desired.height = std::max(desired.height, linePrimary);
    } else {
        desired.width = std::max(desired.width, linePrimary);
        desired.height += lineCross;
    }
    return desired;
}

Size TabPanel::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical = GetIsVertical();
    const double limit = vertical ? finalSize.height : finalSize.width;
    double x = 0.0;
    double y = 0.0;
    double lineCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size size = child->GetDesiredSize();
        const double primary = vertical ? size.height : size.width;
        const double cross = vertical ? size.width : size.height;
        if ((vertical ? y : x) > 0.0 &&
            (vertical ? y : x) + primary > limit) {
            if (vertical) {
                x += lineCross;
                y = 0.0;
            } else {
                y += lineCross;
                x = 0.0;
            }
            lineCross = 0.0;
        }
        ArrangeChild(*child, {
            x, y, size.width, size.height});
        if (vertical) y += size.height;
        else x += size.width;
        lineCross = std::max(lineCross, cross);
    }
    return finalSize;
}

AERO_DESCRIBE(TabItem) {
    using namespace Aero::Meta;
    Register<TabItem>(context)
        .Property(TabItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Factory();
    AERO_ON(TabItem, &TabItem::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
}

AERO_DESCRIBE(TabControl) {
    using namespace Aero::Meta;
    Register<TabControl>(context)
        .Property(TabControl::SelectedContentProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Property(TabControl::SelectedContentTemplateProperty, Base::Ref<Base::Object>{})
        .Property(TabControl::ContentTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(TabControl::TabStripPlacementProperty, Dock::Top, AffectsMeasure)
        .Factory();
}


AERO_DESCRIBE(TabPanel) {
    using namespace Aero::Meta;
    Register<TabPanel>(context)
        .Factory();
}


} // namespace Aero::Controls

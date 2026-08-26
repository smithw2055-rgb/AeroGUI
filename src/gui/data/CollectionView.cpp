#include <Aero/Data/CollectionView.hpp>
#include <Aero/Data/CollectionViewSource.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace Aero::Data {
namespace {

struct DefaultViewEntry {
    Collections::IItemsSource* source = nullptr;
    Ref<CollectionView> view;
};

Base::Vector<CollectionView*>& LiveViews() noexcept {
    static Base::Vector<CollectionView*> views;
    return views;
}

CollectionView* AsCollectionView(
    Collections::IItemsSource* source) noexcept {
    if (source == nullptr) return nullptr;
    Base::Vector<CollectionView*>& live = LiveViews();
    for (std::uint32_t index = 0U; index < live.Size(); ++index) {
        CollectionView* view = live[index];
        if (view != nullptr &&
            static_cast<Collections::IItemsSource*>(view) == source) {
            return view;
        }
    }
    return nullptr;
}

Base::Vector<DefaultViewEntry>& DefaultViews() noexcept {
    static Base::Vector<DefaultViewEntry> views;
    return views;
}

int ComparePropertyValues(
    const Base::Object* left,
    const Base::Object* right,
    StringView propertyName) noexcept {
    auto* leftObject = const_cast<Base::Object*>(left);
    auto* rightObject = const_cast<Base::Object*>(right);
    auto* leftDo = ::Aero::TryCast<DependencyObject>(leftObject);
    auto* rightDo = ::Aero::TryCast<DependencyObject>(rightObject);
    if (leftDo == nullptr || rightDo == nullptr || propertyName.Empty()) {
        return left < right ? -1 : (left > right ? 1 : 0);
    }
    const Meta::DependencyProperty* leftProperty =
        leftDo->PropertyRegistry().Find(
            leftDo->RuntimeType(), propertyName);
    const Meta::DependencyProperty* rightProperty =
        rightDo->PropertyRegistry().Find(
            rightDo->RuntimeType(), propertyName);
    if (leftProperty == nullptr || rightProperty == nullptr) {
        return left < right ? -1 : (left > right ? 1 : 0);
    }
    const Meta::PropertyValue leftValue =
        leftDo->GetValue(leftProperty->Handle());
    const Meta::PropertyValue rightValue =
        rightDo->GetValue(rightProperty->Handle());
    if (leftValue.Kind() == Meta::ValueKind::Double &&
        rightValue.Kind() == Meta::ValueKind::Double) {
        if (leftValue.AsDouble() < rightValue.AsDouble()) return -1;
        if (leftValue.AsDouble() > rightValue.AsDouble()) return 1;
        return 0;
    }
    if (leftValue.Kind() == Meta::ValueKind::String &&
        rightValue.Kind() == Meta::ValueKind::String) {
        return leftValue.AsString().Compare(rightValue.AsString());
    }
    if (leftValue.Kind() == Meta::ValueKind::SignedInteger &&
        rightValue.Kind() == Meta::ValueKind::SignedInteger) {
        if (leftValue.AsSignedInteger() < rightValue.AsSignedInteger()) {
            return -1;
        }
        if (leftValue.AsSignedInteger() > rightValue.AsSignedInteger()) {
            return 1;
        }
        return 0;
    }
    return left < right ? -1 : (left > right ? 1 : 0);
}

} // namespace

CollectionView* CollectionViewSource::GetDefaultView(
    Collections::IItemsSource* source) noexcept {
    if (source == nullptr) return nullptr;
    if (CollectionView* existing = AsCollectionView(source)) {
        return existing;
    }
    Base::Vector<DefaultViewEntry>& views = DefaultViews();
    for (std::uint32_t index = 0U; index < views.Size(); ++index) {
        CollectionView* view = views[index].view.Get();
        if (views[index].source == source) return view;
        if (view != nullptr &&
            static_cast<Collections::IItemsSource*>(view) == source) {
            return view;
        }
    }
    Base::Result<Ref<CollectionView>> created =
        Base::MakeRef<CollectionView>(source);
    if (!created) return nullptr;
    DefaultViewEntry entry;
    entry.source = source;
    entry.view = std::move(created).Value();
    CollectionView* view = entry.view.Get();
    Base::Result<void> stored = views.PushBack(std::move(entry));
    if (!stored) return nullptr;
    return view;
}

CollectionView::CollectionView(
    Collections::IItemsSource* source) noexcept
    : inner_(source),
      innerHandler_(this, &CollectionView::OnInnerChanged) {
    static_cast<void>(LiveViews().PushBack(this));
    if (inner_ != nullptr) {
        inner_->AddItemsChanged(innerHandler_);
    }
    Rebuild();
}

CollectionView::~CollectionView() {
    Base::Vector<CollectionView*>& live = LiveViews();
    for (std::uint32_t index = 0U; index < live.Size(); ++index) {
        if (live[index] == this) {
            live[index] = live.Back();
            live.PopBack();
            break;
        }
    }
    if (inner_ != nullptr) {
        static_cast<void>(inner_->RemoveItemsChanged(innerHandler_));
    }
}

std::uint32_t CollectionView::GetCount() const noexcept {
    return map_.Size();
}

Ref<Base::Object> CollectionView::GetItem(
    std::uint32_t index) const noexcept {
    if (inner_ == nullptr || index >= map_.Size()) {
        return {};
    }
    return inner_->GetItem(map_[index]);
}

void CollectionView::AddItemsChanged(
    const Collections::ItemsChangedHandler& handler) noexcept {
    changed_.Add(handler);
}

bool CollectionView::RemoveItemsChanged(
    const Collections::ItemsChangedHandler& handler) noexcept {
    return changed_.Remove(handler);
}

void CollectionView::SetFilter(CollectionViewFilter filter) noexcept {
    filter_ = std::move(filter);
    Refresh();
}

void CollectionView::SortBy(
    StringView propertyName,
    ListSortDirection direction) noexcept {
    static_cast<void>(sortProperty_.Assign(propertyName));
    sortDirection_ = direction;
    hasSort_ = true;
    Refresh();
}

void CollectionView::ClearSort() noexcept {
    hasSort_ = false;
    sortProperty_.Clear();
    sortDirection_ = ListSortDirection::Ascending;
    Refresh();
}

void CollectionView::Refresh() noexcept {
    Ref<Base::Object> current = GetCurrentItem();
    Rebuild();
    if (current) {
        static_cast<void>(MoveCurrentTo(current.Get()));
    } else {
        currentPosition_ = UINT32_MAX;
    }
    NotifyReset();
}

Ref<Base::Object> CollectionView::GetCurrentItem() const noexcept {
    return GetItem(currentPosition_);
}

bool CollectionView::MoveCurrentTo(const Base::Object* item) noexcept {
    if (item == nullptr) {
        if (currentPosition_ == UINT32_MAX) return false;
        currentPosition_ = UINT32_MAX;
        RaiseCurrentChanged();
        return true;
    }
    for (std::uint32_t index = 0U; index < map_.Size(); ++index) {
        if (GetItem(index).Get() == item) {
            return MoveCurrentToPosition(index);
        }
    }
    return false;
}

bool CollectionView::MoveCurrentToPosition(
    std::uint32_t index) noexcept {
    if (index != UINT32_MAX && index >= map_.Size()) {
        return false;
    }
    if (currentPosition_ == index) return true;
    currentPosition_ = index;
    RaiseCurrentChanged();
    return true;
}

bool CollectionView::MoveCurrentToFirst() noexcept {
    return map_.Empty() ? false : MoveCurrentToPosition(0U);
}

bool CollectionView::MoveCurrentToLast() noexcept {
    return map_.Empty()
        ? false
        : MoveCurrentToPosition(map_.Size() - 1U);
}

bool CollectionView::MoveCurrentToNext() noexcept {
    if (map_.Empty()) return false;
    if (currentPosition_ == UINT32_MAX) {
        return MoveCurrentToPosition(0U);
    }
    if (currentPosition_ + 1U >= map_.Size()) return false;
    return MoveCurrentToPosition(currentPosition_ + 1U);
}

bool CollectionView::MoveCurrentToPrevious() noexcept {
    if (map_.Empty() ||
        currentPosition_ == UINT32_MAX ||
        currentPosition_ == 0U) {
        return false;
    }
    return MoveCurrentToPosition(currentPosition_ - 1U);
}

void CollectionView::AddCurrentChanged(
    const CurrentChangedHandler& handler) noexcept {
    currentChanged_.Add(handler);
}

bool CollectionView::RemoveCurrentChanged(
    const CurrentChangedHandler& handler) noexcept {
    return currentChanged_.Remove(handler);
}

void CollectionView::Rebuild() noexcept {
    map_.Clear();
    if (inner_ == nullptr) return;
    const std::uint32_t count = inner_->GetCount();
    static_cast<void>(map_.Reserve(count));
    for (std::uint32_t index = 0U; index < count; ++index) {
        Ref<Base::Object> item = inner_->GetItem(index);
        if (!Passes(item.Get())) continue;
        static_cast<void>(map_.PushBack(index));
    }
    if (!hasSort_ || map_.Size() < 2U) return;
    std::sort(
        map_.Data(),
        map_.Data() + map_.Size(),
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            Ref<Base::Object> leftItem = inner_->GetItem(left);
            Ref<Base::Object> rightItem = inner_->GetItem(right);
            const int order = ComparePropertyValues(
                leftItem.Get(),
                rightItem.Get(),
                sortProperty_.View());
            return sortDirection_ == ListSortDirection::Descending
                ? order > 0
                : order < 0;
        });
}

bool CollectionView::Passes(const Base::Object* item) const noexcept {
    if (filter_.Empty()) return true;
    return filter_(item);
}

void CollectionView::OnInnerChanged(
    const Collections::ItemsChangedEvent&) noexcept {
    Refresh();
}

void CollectionView::NotifyReset() noexcept {
    if (changed_.Empty()) return;
    changed_.Invoke({
        Collections::ItemsChangeAction::Reset,
        UINT32_MAX,
        UINT32_MAX,
        0U,
        map_.Size()});
}

void CollectionView::RaiseCurrentChanged() noexcept {
    if (!currentChanged_.Empty()) currentChanged_.Invoke();
}

} // namespace Aero::Data

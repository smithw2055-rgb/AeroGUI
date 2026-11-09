#include <Aero/Data/CollectionView.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Layout.hpp>
#include <Aero/TryCast.hpp>
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/TypeRegistryCore.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Data {
namespace {

struct DefaultViewEntry {
    Base::WeakRef<Base::Object> source;
    Collections::IItemsSource* rawSource = nullptr;
    Ref<CollectionView> view;
};

CollectionView* AsCollectionView(
    Collections::IItemsSource* source) noexcept {
    if (source == nullptr) return nullptr;
    Base::Object* obj = source->AsObject();
    return obj != nullptr ? ::Aero::TryCast<CollectionView>(obj) : nullptr;
}

Base::Vector<DefaultViewEntry>& DefaultViews() noexcept {
    static thread_local Base::Vector<DefaultViewEntry> views;
    return views;
}

int CompareValues(const Meta::Value& leftValue, const Meta::Value& rightValue) noexcept {
    if (leftValue.Kind() == Meta::ValueKind::Double &&
        rightValue.Kind() == Meta::ValueKind::Double) {
        const double l = leftValue.AsDouble();
        const double r = rightValue.AsDouble();
        if (l < r) return -1;
        if (l > r) return 1;
        return 0;
    }
    if (leftValue.Type() == ::Aero::Meta::TypeOf<Length>() &&
        rightValue.Type() == ::Aero::Meta::TypeOf<Length>()) {
        const Length* leftLength =
            static_cast<const Length*>(leftValue.AsCustom());
        const Length* rightLength =
            static_cast<const Length*>(rightValue.AsCustom());
        if (leftLength == nullptr || rightLength == nullptr) {
            return 0;
        }
        const double leftPixels = leftLength->isAuto ? 0.0 : leftLength->value;
        const double rightPixels = rightLength->isAuto ? 0.0 : rightLength->value;
        if (leftPixels < rightPixels) return -1;
        if (leftPixels > rightPixels) return 1;
        return 0;
    }
    if (leftValue.Kind() == Meta::ValueKind::String &&
        rightValue.Kind() == Meta::ValueKind::String) {
        return leftValue.AsString().Compare(rightValue.AsString());
    }
    if (leftValue.Kind() == Meta::ValueKind::SignedInteger &&
        rightValue.Kind() == Meta::ValueKind::SignedInteger) {
        if (leftValue.AsSignedInteger() < rightValue.AsSignedInteger()) return -1;
        if (leftValue.AsSignedInteger() > rightValue.AsSignedInteger()) return 1;
        return 0;
    }
    if (leftValue.Kind() == Meta::ValueKind::UnsignedInteger &&
        rightValue.Kind() == Meta::ValueKind::UnsignedInteger) {
        if (leftValue.AsUnsignedInteger() < rightValue.AsUnsignedInteger()) return -1;
        if (leftValue.AsUnsignedInteger() > rightValue.AsUnsignedInteger()) return 1;
        return 0;
    }
    if (leftValue.Kind() == Meta::ValueKind::Boolean &&
        rightValue.Kind() == Meta::ValueKind::Boolean) {
        if (!leftValue.AsBoolean() && rightValue.AsBoolean()) return -1;
        if (leftValue.AsBoolean() && !rightValue.AsBoolean()) return 1;
        return 0;
    }
    return 0;
}

struct SortEntry {
    std::uint32_t sourceIndex = 0U;
    Base::Ref<Base::Object> item;
    Meta::PropertyValue key;
    bool hasKey = false;
};

struct SortPropertyLookupCache {
    const Meta::DependencyPropertyRegistry* dpRegistry = nullptr;
    Meta::TypeId dpOwnerType{};
    const Meta::DependencyProperty* dp = nullptr;
    bool dpSearched = false;
    Meta::TypeId metaOwnerType{};
    const Meta::PropertyInfo* metaProp = nullptr;
    bool metaSearched = false;
};

bool ReadSortPropertyValue(
    const Base::Object* item,
    StringView propertyName,
    const Meta::Registry* metadata,
    SortPropertyLookupCache& cache,
    Meta::PropertyValue& outValue) noexcept {
    if (item == nullptr || propertyName.Empty()) {
        return false;
    }
    auto* mutableObject = const_cast<Base::Object*>(item);
    if (auto* doObject = ::Aero::TryCast<DependencyObject>(mutableObject)) {
        const Meta::DependencyPropertyRegistry& registry =
            DependencyObjectAccess::PropertyRegistry(doObject);
        const Meta::TypeId runtimeType = doObject->RuntimeType();
        if (!cache.dpSearched ||
            cache.dpRegistry != &registry ||
            cache.dpOwnerType != runtimeType) {
            cache.dpRegistry = &registry;
            cache.dpOwnerType = runtimeType;
            cache.dp = registry.Find(runtimeType, propertyName);
            cache.dpSearched = true;
        }
        if (cache.dp != nullptr) {
            outValue = doObject->GetValue(cache.dp->Handle());
            return true;
        }
    }

    // Fallback for ViewModelBase reflection objects when TryCast<DependencyObject> returns nullptr
    if (metadata != nullptr) {
        const Meta::TypeId runtimeType = mutableObject->RuntimeType();
        if (!cache.metaSearched || cache.metaOwnerType != runtimeType) {
            cache.metaOwnerType = runtimeType;
            cache.metaProp = metadata->Types().FindProperty(
                runtimeType, propertyName, true);
            cache.metaSearched = true;
        }
        if (cache.metaProp != nullptr) {
            Base::Result<Meta::Value> val =
                metadata->GetProperty(*mutableObject, cache.metaProp->Id());
            if (val) {
                outValue = std::move(val).Value();
                return true;
            }
        }
    }
    return false;
}

int CompareExtractedSortValues(
    const SortEntry& left,
    const SortEntry& right) noexcept {
    if (left.hasKey && right.hasKey) {
        const int cmp = CompareValues(left.key, right.key);
        if (cmp != 0) return cmp;
    }
    const Base::Object* leftPtr = left.item.Get();
    const Base::Object* rightPtr = right.item.Get();
    return leftPtr < rightPtr ? -1 : (leftPtr > rightPtr ? 1 : 0);
}

} // namespace

CollectionView* CollectionViewSource::GetDefaultView(
    Collections::IItemsSource* source) noexcept {
    if (source == nullptr) return nullptr;
    if (CollectionView* existing = AsCollectionView(source)) {
        return existing;
    }
    Base::Object* sourceObj = source->AsObject();
    Base::Vector<DefaultViewEntry>& views = DefaultViews();
    for (std::uint32_t index = 0U; index < views.Size(); ) {
        if (views[index].rawSource != nullptr) {
            Base::Ref<Base::Object> locked = views[index].source.Lock();
            if (!locked) {
                if (index + 1U < views.Size()) {
                    views[index] = std::move(views.Back());
                }
                views.PopBack();
                continue;
            }
            if (sourceObj != nullptr && locked.Get() == sourceObj) {
                return views[index].view.Get();
            }
        } else {
            CollectionView* view = views[index].view.Get();
            if (view != nullptr &&
                static_cast<Collections::IItemsSource*>(view) == source) {
                return view;
            }
        }
        ++index;
    }
    Ref<CollectionView> created =
        Base::MakeRef<CollectionView>(source);
    if (!created) return nullptr;
    DefaultViewEntry entry;
    if (sourceObj != nullptr) {
        entry.source = Base::WeakRef<Base::Object>(
            Base::Ref<Base::Object>::TryFromBorrowed(*sourceObj));
        entry.rawSource = source;
    }
    entry.view = std::move(created);
    CollectionView* view = entry.view.Get();
    views.PushBack(std::move(entry));
    return view;
}

CollectionView::CollectionView(
    Collections::IItemsSource* source) noexcept
    : inner_(source),
      innerHandler_(this, &CollectionView::OnInnerChanged) {
    if (inner_ != nullptr) {
        Base::Object* obj = inner_->AsObject();
        if (obj != nullptr) {
            innerObj_ = Base::WeakRef<Base::Object>(
                Base::Ref<Base::Object>::TryFromBorrowed(*obj));
        }
        inner_->AddItemsChanged(innerHandler_);
    }
    Rebuild();
}

CollectionView::~CollectionView() {
    if (inner_ != nullptr) {
        if (!innerObj_.Expired()) {
            static_cast<void>(inner_->RemoveItemsChanged(innerHandler_));
        }
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
    if (count == 0U) return;
    static_cast<void>(map_.Reserve(count));
    if (!hasSort_) {
        for (std::uint32_t index = 0U; index < count; ++index) {
            Ref<Base::Object> item = inner_->GetItem(index);
            if (!Passes(item.Get())) continue;
            static_cast<void>(map_.PushBack(index));
        }
        return;
    }

    Base::Vector<SortEntry> entries;
    static_cast<void>(entries.Reserve(count));
    for (std::uint32_t index = 0U; index < count; ++index) {
        Ref<Base::Object> item = inner_->GetItem(index);
        if (!Passes(item.Get())) continue;
        SortEntry entry;
        entry.sourceIndex = index;
        entry.item = std::move(item);
        static_cast<void>(entries.PushBack(std::move(entry)));
    }
    if (entries.Size() < 2U) {
        if (entries.Size() == 1U) {
            static_cast<void>(map_.PushBack(entries[0].sourceIndex));
        }
        return;
    }

    const Meta::Registry* metadata = Meta::CurrentObjectFactory().metadata;
    const StringView propertyName = sortProperty_.View();
    SortPropertyLookupCache lookupCache;
    for (std::uint32_t i = 0U; i < entries.Size(); ++i) {
        SortEntry& entry = entries[i];
        entry.hasKey = ReadSortPropertyValue(
            entry.item.Get(),
            propertyName,
            metadata,
            lookupCache,
            entry.key);
    }

    const bool descending = sortDirection_ == ListSortDirection::Descending;
    std::sort(
        entries.Data(),
        entries.Data() + entries.Size(),
        [descending](const SortEntry& left, const SortEntry& right) noexcept {
            const int order = CompareExtractedSortValues(left, right);
            return descending ? order > 0 : order < 0;
        });

    for (std::uint32_t i = 0U; i < entries.Size(); ++i) {
        static_cast<void>(map_.PushBack(entries[i].sourceIndex));
    }
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

#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Collections {

// The collection contract deliberately lives below Controls: authored
// resources and view-model objects can be an ItemsSource without depending on
// a particular control or visual host.
enum class ItemsChangeAction : std::uint8_t {
    Add = 0U,
    Remove,
    Replace,
    Move,
    Reset,
};

struct ItemsChangedEvent final {
    ItemsChangeAction action = ItemsChangeAction::Reset;
    std::uint32_t oldIndex = UINT32_MAX;
    std::uint32_t newIndex = UINT32_MAX;
    std::uint32_t oldCount = 0U;
    std::uint32_t newCount = 0U;
};

using ItemsChangedHandler =
    Base::Delegate<void(const ItemsChangedEvent&)>;

class AERO_API IItemsSource {
public:
    virtual ~IItemsSource() = default;
    virtual std::uint32_t GetCount() const noexcept = 0;
    virtual Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept = 0;
    virtual Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
    virtual bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
};

// Reference-counted observable collection used by view models and bindings.
// It deliberately lives in Collections rather than Controls so a data source
// does not acquire a dependency on visual controls or item containers.
class AERO_API ObservableCollection final :
    public Base::Object,
    public IItemsSource {
    AERO_DECLARE_TYPE(ObservableCollection, Base::Object)
public:
    ObservableCollection() noexcept = default;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t GetCount() const noexcept override {
        return items_.Size();
    }
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < items_.Size() ? items_[index] : Base::Ref<Base::Object>{};
    }
    Base::Result<void> TryAdd(
        Base::Ref<Base::Object> item) noexcept {
        return TryInsert(items_.Size(), std::move(item));
    }
    Base::Result<void> TryInsert(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept {
        if (!item || index > items_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ObservableCollection insert is invalid");
        }
        Base::Result<void> reserved =
            items_.TryReserve(items_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
        Base::Ref<Base::Object> placeholder;
        Base::Result<void> pushed = items_.TryPushBack(std::move(placeholder));
        if (!pushed) return pushed.GetStatus();
        for (std::uint32_t current = items_.Size() - 1U;
             current > index; --current) {
            items_[current] = std::move(items_[current - 1U]);
        }
        items_[index] = std::move(item);
        if (!changed_.Empty()) changed_.Invoke({
            ItemsChangeAction::Add,
            UINT32_MAX,
            index,
            0U,
            1U});
        return {};
    }
    Base::Result<Base::Ref<Base::Object>> TryRemoveAt(
        std::uint32_t index) noexcept {
        if (index >= items_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "ObservableCollection remove index is out of range");
        }
        Base::Ref<Base::Object> removed = std::move(items_[index]);
        for (std::uint32_t current = index + 1U;
             current < items_.Size(); ++current) {
            items_[current - 1U] = std::move(items_[current]);
        }
        items_.PopBack();
        if (!changed_.Empty()) changed_.Invoke({
            ItemsChangeAction::Remove,
            index,
            UINT32_MAX,
            1U,
            0U});
        return removed;
    }
    void Reset() noexcept {
        const std::uint32_t oldCount = items_.Size();
        items_.Clear();
        if (oldCount != 0U && !changed_.Empty()) changed_.Invoke({
            ItemsChangeAction::Reset,
            UINT32_MAX,
            UINT32_MAX,
            oldCount,
            0U});
    }
    Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }
    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }

private:
    Base::Vector<Base::Ref<Base::Object>> items_;
    ItemsChangedHandler changed_;
};

} // namespace Aero::Collections

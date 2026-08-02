#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Collections {

// The collection contract deliberately lives below Controls: authored
// resources and view-model objects can be an ItemsSource without depending on
// a particular control or visual host.
class AERO_API IItemsSource {
public:
    virtual ~IItemsSource() = default;
    virtual std::uint32_t GetCount() const noexcept = 0;
    virtual Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept = 0;
    virtual void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
    virtual bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
};

// Reference-counted observable collection used by view models and bindings.
// It deliberately lives in Collections rather than Controls so a data source
// does not acquire a dependency on visual controls or item containers.
class AERO_API ObservableCollection :
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
    Base::Result<void> Add(
        Base::Ref<Base::Object> item) noexcept {
        return Insert(items_.Size(), std::move(item));
    }
    Base::Result<void> Insert(
        std::uint32_t index,
        Base::Ref<Base::Object> item) noexcept {
        if (!item || index > items_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ObservableCollection insert is invalid");
        }
        Base::Result<void> reserved =
            items_.Reserve(items_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
        Base::Ref<Base::Object> placeholder;
        Base::Result<void> pushed = items_.PushBack(std::move(placeholder));
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
    Base::Result<Base::Ref<Base::Object>> RemoveAt(
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
    void AddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
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

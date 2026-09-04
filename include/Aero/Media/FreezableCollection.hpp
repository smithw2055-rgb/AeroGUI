#pragma once

#include <Aero/Collections.hpp>
#include <Aero/Freezable.hpp>

#include <type_traits>
#include <utility>

namespace Aero::Media {

// WPF-shaped Freezable collection. Template has no AERO_GUI_API; typed
// wrappers may add TypeIds when a XAML name is required.
template<class T>
class FreezableCollection :
    public Freezable,
    public Collections::IItemsSource {
    static_assert(
        std::is_base_of<Freezable, T>::value,
        "FreezableCollection<T> requires a Freezable item type");
public:
    FreezableCollection() noexcept
        : Freezable(Freezable::StaticTypeId()) {}
    explicit FreezableCollection(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    ~FreezableCollection() override {
        DetachHandlers();
    }

    Span<const Ref<T>> GetItems() const noexcept {
        return items_.AsSpan();
    }
    Span<const Ref<T>> AsSpan() const noexcept {
        return items_.AsSpan();
    }
    std::uint32_t GetCount() const noexcept override {
        return items_.Size();
    }
    std::uint32_t Size() const noexcept { return items_.Size(); }
    bool Empty() const noexcept { return items_.Empty(); }
    Ref<T>* begin() noexcept { return items_.begin(); }
    const Ref<T>* begin() const noexcept { return items_.begin(); }
    Ref<T>* end() noexcept { return items_.end(); }
    const Ref<T>* end() const noexcept { return items_.end(); }
    Base::Object* AsObject() noexcept override { return this; }
    Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < items_.Size()
            ? Ref<Base::Object>(items_[index])
            : Ref<Base::Object>{};
    }
    Result<void> Add(Ref<T> item) noexcept {
        Result<void> writable = WritePreamble();
        if (!writable) return writable.GetStatus();
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "FreezableCollection item cannot be null");
        }
        EnsureItemHandler();
        T* retained = item.Get();
        if (!retained->IsFrozen()) {
            Result<void> subscribed =
                retained->AddChangedHandler(itemChangedHandler_);
            if (!subscribed) return subscribed.GetStatus();
        }
        Result<void> added = items_.PushBack(std::move(item));
        if (!added) {
            if (!retained->IsFrozen() && !itemChangedHandler_.Empty()) {
                static_cast<void>(retained->RemoveChangedHandler(
                    itemChangedHandler_));
            }
            return added.GetStatus();
        }
        if (!changed_.Empty()) {
            changed_.Invoke({
                Collections::ItemsChangeAction::Add,
                UINT32_MAX,
                items_.Size() - 1U,
                0U,
                1U});
        }
        WritePostscript();
        return {};
    }
    void Clear() noexcept {
        if (!WritePreamble() || items_.Empty()) return;
        const std::uint32_t oldCount = items_.Size();
        DetachHandlers();
        items_.Clear();
        if (oldCount != 0U && !changed_.Empty()) {
            changed_.Invoke({
                Collections::ItemsChangeAction::Reset,
                UINT32_MAX,
                UINT32_MAX,
                oldCount,
                0U});
        }
        WritePostscript();
    }
    void AddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        if (IsFrozen()) return;
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }

protected:
    bool FreezeCore(bool isChecking) noexcept override {
        for (Ref<T>& item : items_) {
            if (!item) continue;
            if (isChecking) {
                if (!item->CanFreeze()) return false;
            } else {
                static_cast<void>(item->Freeze());
            }
        }
        return Freezable::FreezeCore(isChecking);
    }

private:
    void EnsureItemHandler() noexcept {
        if (itemChangedHandler_.Empty()) {
            itemChangedHandler_ = FreezableChangedHandler(
                this, &FreezableCollection::OnItemChanged);
        }
    }
    void DetachHandlers() noexcept {
        if (itemChangedHandler_.Empty()) return;
        for (Ref<T>& item : items_) {
            if (item && !item->IsFrozen()) {
                static_cast<void>(item->RemoveChangedHandler(
                    itemChangedHandler_));
            }
        }
    }
    void OnItemChanged(Freezable&) noexcept {
        WritePostscript();
    }

    Base::Vector<Ref<T>> items_;
    Collections::ItemsChangedHandler changed_;
    FreezableChangedHandler itemChangedHandler_;
};

} // namespace Aero::Media

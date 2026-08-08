#pragma once

#include <Aero/Collections.hpp>
#include <Aero/Value.hpp>

namespace Aero::Controls {

using ItemsChangeAction = Collections::ItemsChangeAction;
using ItemsChangedEvent = Collections::ItemsChangedEvent;
using ItemsChangedHandler = Collections::ItemsChangedHandler;

class AERO_GUI_API ItemCollection : public Collections::IItemsSource {
public:
    std::uint32_t GetCount() const noexcept override { return items_.Size(); }
    Base::Ref<Base::Object> GetItem(std::uint32_t index) const noexcept override;
    Base::Result<void> Add(Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Insert(
        std::uint32_t index, Base::Ref<Base::Object> item) noexcept;
    Base::Result<Base::Ref<Base::Object>> RemoveAt(
        std::uint32_t index) noexcept;
    Base::Result<void> Replace(
        std::uint32_t index, Base::Ref<Base::Object> item) noexcept;
    Base::Result<void> Move(
        std::uint32_t oldIndex, std::uint32_t newIndex) noexcept;
    void Reset() noexcept;
    Base::Result<void> Reset(
        Base::Span<const Base::Ref<Base::Object>> items) noexcept;
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
    void Notify(const ItemsChangedEvent& event) noexcept;
};

AERO_GUI_API Base::Result<void> AddBoxedItem(
    Collections::ObservableCollection& source, Value value) noexcept;
AERO_GUI_API Base::Result<void> AddBoxedStringItem(
    Collections::ObservableCollection& source,
    Base::StringView value) noexcept;

} // namespace Aero::Controls

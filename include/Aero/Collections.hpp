#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>

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
    virtual std::uint32_t Count() const noexcept = 0;
    virtual Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept = 0;
    virtual Base::Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
    virtual bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept = 0;
};

} // namespace Aero::Collections

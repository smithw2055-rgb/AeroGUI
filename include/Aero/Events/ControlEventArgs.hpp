#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Events/EventArgs.hpp>

#include <cstdint>

namespace Aero::Collections {

enum class ItemsChangeAction : std::uint8_t {
    Add = 0U,
    Remove,
    Replace,
    Move,
    Reset,
};

struct ItemsChangedEvent {
    ItemsChangeAction action{};
    std::uint32_t oldIndex = UINT32_MAX;
    std::uint32_t newIndex = UINT32_MAX;
    std::uint32_t oldCount = 0U;
    std::uint32_t newCount = 0U;
};

using ItemsChangedHandler = Base::Delegate<void(const ItemsChangedEvent&)>;

} // namespace Aero::Collections

namespace Aero::Controls {

enum class ScrollInputKind : std::uint8_t {
    Line = 0U,
    Page,
    Wheel,
    Thumb,
    Touch,
};

struct ScrollData {
    double horizontalOffset = 0.0;
    double verticalOffset = 0.0;
    double extentWidth = 0.0;
    double extentHeight = 0.0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

struct ScrollChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(ScrollChangedEventArgs, RoutedEventArgs)
public:
    ScrollChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    ScrollChangedEventArgs(
        ScrollData oldData,
        ScrollData newData,
        ScrollInputKind inputKind) noexcept
        : RoutedEventArgs(StaticTypeId()),
          oldData_(oldData), newData_(newData), inputKind_(inputKind) {}

    ScrollData GetOldData() const noexcept { return oldData_; }
    ScrollData GetNewData() const noexcept { return newData_; }
    ScrollInputKind GetInputKind() const noexcept { return inputKind_; }

private:
    ScrollData oldData_;
    ScrollData newData_;
    ScrollInputKind inputKind_ = ScrollInputKind::Line;
};

using ScrollChangedEventHandler = Base::Delegate<void(
    Base::Object*, ScrollChangedEventArgs&)>;

struct RangeValueChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(RangeValueChangedEventArgs, RoutedEventArgs)
public:
    RangeValueChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    RangeValueChangedEventArgs(
        double oldValue,
        double newValue) noexcept
        : RoutedEventArgs(StaticTypeId()),
          oldValue_(oldValue), newValue_(newValue) {}

    double GetOldValue() const noexcept { return oldValue_; }
    double GetNewValue() const noexcept { return newValue_; }

private:
    double oldValue_ = 0.0;
    double newValue_ = 0.0;
};

using RangeValueChangedEventHandler = Base::Delegate<void(
    Base::Object*, RangeValueChangedEventArgs&)>;

namespace Primitives { class Selector; }

struct SelectionChangedEvent {
    Base::Span<const std::uint32_t> removedIndices;
    Base::Span<const std::uint32_t> addedIndices;
    std::uint32_t oldPrimaryIndex = UINT32_MAX;
    std::uint32_t newPrimaryIndex = UINT32_MAX;
    Base::Ref<Base::Object> oldPrimaryItem;
    Base::Ref<Base::Object> newPrimaryItem;
};

using SelectionChangedHandler = Base::Delegate<void(
    Primitives::Selector&, const SelectionChangedEvent&)>;

} // namespace Aero::Controls

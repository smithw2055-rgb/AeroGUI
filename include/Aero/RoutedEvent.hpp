#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/RoutedEvent.hpp>
#include <Aero/Input/Values.hpp>

#include <cstdint>

namespace Aero {

class UIElement;

using RoutedEvent = Core::RoutedEventHandle;
using RoutedEventHandle = Core::RoutedEventHandle;
using RoutingStrategy = Core::RoutingStrategy;

template<class TOwner, class TArgs>
using RoutedEventRef = Core::RoutedEventRef<TOwner, TArgs>;

struct EventArgs {
    AERO_DECLARE_TYPE(EventArgs, Core::NoMetadataBase)
    explicit constexpr EventArgs(Core::TypeId type = StaticTypeId()) noexcept : eventArgsType(type) {}
    Core::TypeId eventArgsType = StaticTypeId();
};

struct RoutedEventArgs : EventArgs {
    AERO_DECLARE_TYPE(RoutedEventArgs, EventArgs)
    explicit constexpr RoutedEventArgs(Core::TypeId type = StaticTypeId()) noexcept : EventArgs(type) {}
    RoutedEvent routedEvent;
    Base::Object* source = nullptr;
    Base::Object* originalSource = nullptr;
    mutable bool handled = false;
};

struct InputEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(InputEventArgs, RoutedEventArgs)
    explicit constexpr InputEventArgs(Core::TypeId type = StaticTypeId()) noexcept : RoutedEventArgs(type) {}
    std::uint32_t modifiers = 0U;
};

struct MouseEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(MouseEventArgs, InputEventArgs)
    explicit constexpr MouseEventArgs(Core::TypeId type = StaticTypeId()) noexcept : InputEventArgs(type) {}
    std::uint32_t pointerId = 0U;
    Base::Point position;
};

struct MouseButtonEventArgs final : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseButtonEventArgs, MouseEventArgs)
    constexpr MouseButtonEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}
    Input::MouseButton changedButton = Input::MouseButton::Left;
    Input::MouseButtonState buttonState = Input::MouseButtonState::Released;
};

struct MouseWheelEventArgs final : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseWheelEventArgs, MouseEventArgs)
    constexpr MouseWheelEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}
    double deltaX = 0.0;
    double deltaY = 0.0;
};

struct KeyEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE(KeyEventArgs, InputEventArgs)
    constexpr KeyEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    Input::KeyboardAction action = Input::KeyboardAction::Down;
    std::uint32_t key = 0U;
    bool isRepeat = false;
};

struct TextCompositionEventArgs final : InputEventArgs {
    AERO_DECLARE_TYPE(TextCompositionEventArgs, InputEventArgs)
    constexpr TextCompositionEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}
    Base::StringView text;
};

struct KeyboardFocusChangedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(KeyboardFocusChangedEventArgs, RoutedEventArgs)
    constexpr KeyboardFocusChangedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}
    UIElement* oldFocus = nullptr;
    UIElement* newFocus = nullptr;
};

using EventHandler = Base::Delegate<void(Base::Object*, const EventArgs&)>;
using RoutedEventHandler = Base::Delegate<void(Base::Object*, const RoutedEventArgs&)>;
using MouseEventHandler = Base::Delegate<void(Base::Object*, const MouseEventArgs&)>;
using MouseButtonEventHandler = Base::Delegate<void(Base::Object*, const MouseButtonEventArgs&)>;
using MouseWheelEventHandler = Base::Delegate<void(Base::Object*, const MouseWheelEventArgs&)>;
using KeyEventHandler = Base::Delegate<void(Base::Object*, const KeyEventArgs&)>;
using TextCompositionEventHandler = Base::Delegate<void(Base::Object*, const TextCompositionEventArgs&)>;
using KeyboardFocusChangedEventHandler = Base::Delegate<void(Base::Object*, const KeyboardFocusChangedEventArgs&)>;

} // namespace Aero

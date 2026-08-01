#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Input/Values.hpp>
#include <Aero/Meta/MetadataId.hpp>

#include <cstdint>

namespace Aero {

class UIElement;

using RoutedEventId = Core::MemberId;

struct RoutedEventHandle final {
    RoutedEventId value = Core::InvalidMemberId;
    constexpr bool IsValid() const noexcept { return value != Core::InvalidMemberId; }
};

using RoutedEvent = RoutedEventHandle;

enum class RoutingStrategy : std::uint8_t { Direct = 0U, Tunnel, Bubble };

constexpr RoutedEventHandle MakeRoutedEventHandle(Core::TypeId ownerType, Base::StringView name) noexcept;

template<class TOwner, class TArgs>
class RoutedEventRef final {
public:
    using Owner = TOwner;
    using Args = TArgs;

    constexpr explicit RoutedEventRef(Base::StringView name) noexcept
        : name_(name), handle_(MakeRoutedEventHandle(TOwner::StaticTypeIdValue_, name)) {}

    constexpr Base::StringView Name() const noexcept { return name_; }
    constexpr RoutedEventHandle Handle() const noexcept { return handle_; }
    constexpr operator RoutedEventHandle() const noexcept { return handle_; }
    constexpr Core::MemberId Id() const noexcept { return handle_.value; }

private:
    Base::StringView name_;
    RoutedEventHandle handle_;
};

constexpr bool operator==(RoutedEventHandle left, RoutedEventHandle right) noexcept { return left.value == right.value; }
constexpr bool operator!=(RoutedEventHandle left, RoutedEventHandle right) noexcept { return !(left == right); }

constexpr RoutedEventHandle MakeRoutedEventHandle(Core::TypeId ownerType, Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.MEMBER.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(2U);
    builder.AddString(name);
    return {builder.Finish()};
}

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
    bool handled = false;
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

using EventHandler = Base::Delegate<void(Base::Object*, EventArgs&)>;
using RoutedEventHandler = Base::Delegate<void(Base::Object*, RoutedEventArgs&)>;
using MouseEventHandler = Base::Delegate<void(Base::Object*, MouseEventArgs&)>;
using MouseButtonEventHandler = Base::Delegate<void(Base::Object*, MouseButtonEventArgs&)>;
using MouseWheelEventHandler = Base::Delegate<void(Base::Object*, MouseWheelEventArgs&)>;
using KeyEventHandler = Base::Delegate<void(Base::Object*, KeyEventArgs&)>;
using TextCompositionEventHandler = Base::Delegate<void(Base::Object*, TextCompositionEventArgs&)>;
using KeyboardFocusChangedEventHandler = Base::Delegate<void(Base::Object*, KeyboardFocusChangedEventArgs&)>;

} // namespace Aero

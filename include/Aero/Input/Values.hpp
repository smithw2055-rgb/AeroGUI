#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cstdint>

namespace Aero { class UIElement; }

namespace Aero::Input {

enum class InputScope : std::uint8_t {
    Default = 0U,
    Url,
    EmailSmtpAddress,
    Digits,
    Number,
    Password,
    TelephoneNumber
};

enum class PointerAction : std::uint8_t { Move = 0U, Down, Up, Wheel };
enum class KeyboardAction : std::uint8_t { Down = 0U, Up };
enum class MouseButton : std::uint8_t {
    Left = 0U, Right, Middle, XButton1, XButton2
};
enum class MouseButtonState : std::uint8_t { Released = 0U, Pressed };

struct HitTestResult final {
    Aero::UIElement* target = nullptr;
    Base::Point position;
    bool HasTarget() const noexcept { return target != nullptr; }
};

struct PointerInput final {
    std::uint32_t pointerId = 0U;
    PointerAction action = PointerAction::Move;
    Base::Point position;
    MouseButton changedButton = MouseButton::Left;
    double wheelDeltaX = 0.0;
    double wheelDeltaY = 0.0;
};

struct PointerDispatchResult final {
    HitTestResult hit;
    bool routed = false;
};

struct KeyboardInput final {
    KeyboardAction action = KeyboardAction::Down;
    std::uint32_t key = 0U;
    std::uint32_t modifiers = 0U;
    bool isRepeat = false;
};

inline constexpr std::uint32_t KeyboardKeyTab = 9U;
inline constexpr std::uint32_t KeyboardKeyBackspace = 8U;
inline constexpr std::uint32_t KeyboardKeyEnter = 13U;
inline constexpr std::uint32_t KeyboardKeyEscape = 27U;
inline constexpr std::uint32_t KeyboardKeySpace = 32U;
inline constexpr std::uint32_t KeyboardKeyHome = 0x24U;
inline constexpr std::uint32_t KeyboardKeyEnd = 0x23U;
inline constexpr std::uint32_t KeyboardKeyLeft = 0x25U;
inline constexpr std::uint32_t KeyboardKeyUp = 0x26U;
inline constexpr std::uint32_t KeyboardKeyRight = 0x27U;
inline constexpr std::uint32_t KeyboardKeyDown = 0x28U;
inline constexpr std::uint32_t KeyboardKeyDelete = 0x2EU;
inline constexpr std::uint32_t KeyboardKeyA = 0x41U;
inline constexpr std::uint32_t KeyboardKeyC = 0x43U;
inline constexpr std::uint32_t KeyboardKeyV = 0x56U;
inline constexpr std::uint32_t KeyboardKeyX = 0x58U;
inline constexpr std::uint32_t KeyboardKeyY = 0x59U;
inline constexpr std::uint32_t KeyboardKeyZ = 0x5AU;

enum class KeyboardModifiers : std::uint32_t {
    None = 0U,
    Shift = 1U << 0U,
    Control = 1U << 1U,
    Alt = 1U << 2U
};

constexpr bool HasKeyboardModifier(
    std::uint32_t modifiers,
    KeyboardModifiers value) noexcept {
    return (modifiers &
        static_cast<std::uint32_t>(value)) != 0U;
}

struct KeyboardDispatchResult final {
    Aero::UIElement* target = nullptr;
    bool routed = false;
    bool commandExecuted = false;
    bool focusMoved = false;
};

struct TextInput final {
    Base::StringView text;
};

struct TextInputDispatchResult final {
    Aero::UIElement* target = nullptr;
    bool routed = false;
};

} // namespace Aero::Input

namespace Aero::Core {

template<>
struct MetaTypeTraits<Aero::Input::InputScope> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("InputScope"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "InputScope"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core

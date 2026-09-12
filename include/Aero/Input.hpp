#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/InputScope.hpp>
#include <cstdint>

namespace Aero { class UIElement; }

namespace Aero::Input {

enum class PointerAction : std::uint8_t { Move = 0U, Down, Up, Wheel };
enum class KeyboardAction : std::uint8_t { Down = 0U, Up };
enum class MouseButton : std::uint8_t {
    Left = 0U, Right, Middle, XButton1, XButton2
};
enum class MouseButtonState : std::uint8_t { Released = 0U, Pressed };

enum class DragDropEffects : std::uint8_t {
    None = 0U,
    Copy = 1U << 0U,
    Move = 1U << 1U,
    Link = 1U << 2U,
    All = Copy | Move | Link
};

constexpr DragDropEffects operator|(
    DragDropEffects left, DragDropEffects right) noexcept {
    return static_cast<DragDropEffects>(
        static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

constexpr DragDropEffects operator&(
    DragDropEffects left, DragDropEffects right) noexcept {
    return static_cast<DragDropEffects>(
        static_cast<std::uint8_t>(left) &
        static_cast<std::uint8_t>(right));
}

constexpr bool HasDragDropEffect(
    DragDropEffects value, DragDropEffects effect) noexcept {
    return (value & effect) != DragDropEffects::None;
}

struct HitTestResult {
    Aero::UIElement* target = nullptr;
    Base::Point position;
    bool HasTarget() const noexcept { return target != nullptr; }
};

struct PointerInput {
    std::uint32_t pointerId = 0U;
    PointerAction action = PointerAction::Move;
    Base::Point position;
    MouseButton changedButton = MouseButton::Left;
    std::uint32_t clickCount = 1U;
    double wheelDeltaX = 0.0;
    double wheelDeltaY = 0.0;
};

struct PointerDispatchResult {
    HitTestResult hit;
    bool routed = false;
};

struct KeyboardInput {
    KeyboardAction action = KeyboardAction::Down;
    std::uint32_t key = 0U;
    std::uint32_t modifiers = 0U;
    bool isRepeat = false;
};

enum class Key : std::uint32_t {
    None = 0U,
    Back = 8U,
    Tab = 9U,
    Enter = 13U,
    Escape = 27U,
    Space = 32U,
    End = 0x23U,
    Home = 0x24U,
    Left = 0x25U,
    Up = 0x26U,
    Right = 0x27U,
    Down = 0x28U,
    PageUp = 0x21U,
    PageDown = 0x22U,
    Delete = 0x2EU,
    A = 0x41U,
    C = 0x43U,
    V = 0x56U,
    X = 0x58U,
    Y = 0x59U,
    Z = 0x5AU
};

inline constexpr std::uint32_t KeyboardKeyTab = 9U;
inline constexpr std::uint32_t KeyboardKeyBackspace = 8U;
inline constexpr std::uint32_t KeyboardKeyEnter = 13U;
inline constexpr std::uint32_t KeyboardKeyEscape = 27U;
inline constexpr std::uint32_t KeyboardKeySpace = 32U;
inline constexpr std::uint32_t KeyboardKeyHome = 0x24U;
inline constexpr std::uint32_t KeyboardKeyEnd = 0x23U;
inline constexpr std::uint32_t KeyboardKeyPageUp = 0x21U;
inline constexpr std::uint32_t KeyboardKeyPageDown = 0x22U;
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

struct KeyboardDispatchResult {
    Aero::UIElement* target = nullptr;
    bool routed = false;
    bool commandExecuted = false;
    bool focusMoved = false;
};

struct TextInput {
    StringView text;
};

struct TextInputDispatchResult {
    Aero::UIElement* target = nullptr;
    bool routed = false;
};

} // namespace Aero::Input

// Device/input-type headers (Mouse, Keyboard, Cursor, Cursors, DataObject,
// DragDrop) depend on the enums/aliases declared above (KeyboardModifiers,
// DragDropEffects). They are included at file scope (each reopens
// namespace Aero::Input) so the declarations are available to consumers that
// include only <Aero/Input.hpp>.
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Cursors.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Input/Mouse.hpp>

AERO_DECLARE_TYPE_ENUM(Aero::Input::DragDropEffects)

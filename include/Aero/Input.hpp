#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <utility>

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

// WPF-shaped key identifiers for the host-friendly View input API. Values
// intentionally match the platform-neutral key codes already used internally.
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

AERO_DECLARE_TYPE_ENUM(Aero::Input::InputScope)
AERO_DECLARE_TYPE_ENUM(Aero::Input::DragDropEffects)

#include <Aero/RoutedEvent.hpp>

namespace Aero::Input {

using CanExecuteChangedHandler = Base::Delegate<void()>;

class AERO_GUI_API ICommand : public Base::Object {
    AERO_DECLARE_TYPE(ICommand, Base::Object)
public:
    ~ICommand() override = default;

    virtual Result<bool> CanExecute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept = 0;
    virtual void Execute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept = 0;

    void AddCanExecuteChanged(
        const CanExecuteChangedHandler& handler) noexcept;
    bool RemoveCanExecuteChanged(
        const CanExecuteChangedHandler& handler) noexcept;

protected:
    ICommand() noexcept = default;
    void RaiseCanExecuteChanged() const noexcept;

private:
    CanExecuteChangedHandler canExecuteChanged_;
};

class AERO_GUI_API InputGesture : public Base::Object {
    AERO_DECLARE_TYPE(InputGesture, Base::Object)
public:
    ~InputGesture() override = default;
    virtual bool Matches(const KeyboardInput& input) const noexcept = 0;

protected:
    InputGesture() noexcept = default;
};

class AERO_GUI_API KeyGesture : public InputGesture {
    AERO_DECLARE_TYPE(KeyGesture, InputGesture)
public:
    KeyGesture() noexcept = default;
    KeyGesture(std::uint32_t key, std::uint32_t modifiers = 0U) noexcept
        : key_(key), modifiers_(modifiers) {}

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t GetKey() const noexcept { return key_; }
    std::uint32_t GetModifiers() const noexcept { return modifiers_; }
    bool IsValid() const noexcept { return key_ != 0U; }
    bool Matches(const KeyboardInput& input) const noexcept override;

private:
    std::uint32_t key_ = 0U;
    std::uint32_t modifiers_ = 0U;
};

class AERO_GUI_API RoutedCommand : public ICommand {
    AERO_DECLARE_TYPE(RoutedCommand, ICommand)
public:
    RoutedCommand() noexcept;
    explicit RoutedCommand(StringView name) noexcept;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetName() const noexcept { return name_.View(); }
    void SetName(StringView name) noexcept;
    void AddInputGesture(Ref<InputGesture> gesture) noexcept;
    Span<const Ref<InputGesture>> GetInputGestures() const noexcept {
        return {gestures_.Data(), gestures_.Size()};
    }
    bool MatchesInput(const KeyboardInput& input) const noexcept;

    Result<bool> CanExecute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept override;
    void Execute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept override;

    void InvalidateCanExecute() const noexcept {
        RaiseCanExecuteChanged();
    }

    // Registers and resolves process-stable WPF-style static command members.
    // Module metadata registers the supported names once; XAML and control
    // bindings then resolve the same command object identity.
    static Result<void> RegisterStatic(
        Meta::TypeId ownerType,
        StringView memberName) noexcept;
    static Result<Ref<RoutedCommand>> ResolveStatic(
        Meta::TypeId ownerType,
        StringView memberName) noexcept;

private:
    friend class KeyBinding;

    Result<void> AssignNameChecked(StringView name) noexcept;
    Result<void> AddInputGestureChecked(
        Ref<InputGesture> gesture) noexcept;

    String name_;
    Base::Vector<Ref<InputGesture>> gestures_;
};

// XAML-authored keyboard binding. The WPF command spelling (for example
// ApplicationCommands.New) is retained while runtime input uses a concrete
// RoutedCommand and KeyGesture.
class AERO_GUI_API KeyBinding : public Base::Object {
    AERO_DECLARE_TYPE(KeyBinding, Base::Object)
public:
    KeyBinding() noexcept = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    StringView GetKeyName() const noexcept { return keyName_.View(); }
    StringView GetModifiersName() const noexcept { return modifiersName_.View(); }
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    Result<void> Finalize() noexcept;

private:
    String commandName_;
    String keyName_;
    String modifiersName_;
    Ref<RoutedCommand> command_;
};

class AERO_GUI_API CommandBinding {
public:
    CommandBinding() noexcept = default;
    CommandBinding(
        Ref<RoutedCommand> command,
        const ExecutedRoutedEventHandler& executed,
        const CanExecuteRoutedEventHandler& canExecute = {}) noexcept
        : command_(std::move(command)),
          canExecute_(canExecute),
          executed_(executed) {}

    RoutedCommand* GetCommand() const noexcept { return command_.Get(); }
    const CanExecuteRoutedEventHandler& GetCanExecute() const noexcept {
        return canExecute_;
    }
    const ExecutedRoutedEventHandler& GetExecuted() const noexcept {
        return executed_;
    }
    bool IsValid() const noexcept {
        return command_ && (!canExecute_.Empty() || !executed_.Empty());
    }

private:
    Ref<RoutedCommand> command_;
    CanExecuteRoutedEventHandler canExecute_;
    ExecutedRoutedEventHandler executed_;
};

} // namespace Aero::Input

namespace Aero::Input {



// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.

enum class FocusNavigationDirection : std::uint8_t {
    Next,
    Previous,
};

enum class KeyboardNavigationMode : std::uint8_t {
    Continue = 0U,
    Once,
    Cycle,
    None,
    Contained,
    Local
};

class AERO_GUI_API KeyboardNavigation
    : public Base::Object {
    AERO_DECLARE_TYPE(
        KeyboardNavigation,
        Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr AttachedProperty<KeyboardNavigationMode> DirectionalNavigationProperty{"DirectionalNavigation"};

    inline static constexpr AttachedProperty<KeyboardNavigationMode> TabNavigationProperty{"TabNavigation"};
    inline static constexpr AttachedProperty<KeyboardNavigationMode> ControlTabNavigationProperty{"ControlTabNavigation"};
    inline static constexpr AttachedProperty<std::uint32_t> TabIndexProperty{"TabIndex"};
};

// Focus scopes are authored as attached properties in WPF.
class AERO_GUI_API FocusManager : public Base::Object {
    AERO_DECLARE_TYPE(FocusManager, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr AttachedProperty<bool> IsFocusScopeProperty{"IsFocusScope"};
    inline static constexpr AttachedProperty<Ref<Base::Object>> FocusedElementProperty{"FocusedElement"};
};


// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.


} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::KeyboardNavigationMode)

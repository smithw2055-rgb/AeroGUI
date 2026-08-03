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
    Base::StringView text;
};

struct TextInputDispatchResult {
    Aero::UIElement* target = nullptr;
    bool routed = false;
};

} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::InputScope)

#include <Aero/Events/RoutedEvent.hpp>

namespace Aero::Input {

using CanExecuteChangedHandler = Base::Delegate<void()>;

class AERO_API ICommand : public Base::Object {
    AERO_DECLARE_TYPE(ICommand, Base::Object)
public:
    ~ICommand() override = default;

    virtual Base::Result<bool> CanExecute(
        const Meta::Value& parameter,
        UIElement* target = nullptr) noexcept = 0;
    virtual void Execute(
        const Meta::Value& parameter,
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

class AERO_API InputGesture : public Base::Object {
    AERO_DECLARE_TYPE(InputGesture, Base::Object)
public:
    ~InputGesture() override = default;
    virtual bool Matches(const KeyboardInput& input) const noexcept = 0;

protected:
    InputGesture() noexcept = default;
};

class AERO_API KeyGesture : public InputGesture {
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

class AERO_API RoutedCommand : public ICommand {
    AERO_DECLARE_TYPE(RoutedCommand, ICommand)
public:
    RoutedCommand() noexcept;
    explicit RoutedCommand(Base::StringView name) noexcept;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetName() const noexcept { return name_.View(); }
    void SetName(Base::StringView name) noexcept;
    void AddInputGesture(Base::Ref<InputGesture> gesture) noexcept;
    Base::Span<const Base::Ref<InputGesture>> GetInputGestures() const noexcept {
        return {gestures_.Data(), gestures_.Size()};
    }
    bool MatchesInput(const KeyboardInput& input) const noexcept;

    Base::Result<bool> CanExecute(
        const Meta::Value& parameter,
        UIElement* target = nullptr) noexcept override;
    void Execute(
        const Meta::Value& parameter,
        UIElement* target = nullptr) noexcept override;

    void InvalidateCanExecute() const noexcept {
        RaiseCanExecuteChanged();
    }

private:
    friend class KeyBinding;

    Base::Result<void> AssignNameChecked(Base::StringView name) noexcept;
    Base::Result<void> AddInputGestureChecked(
        Base::Ref<InputGesture> gesture) noexcept;

    Base::String name_;
    Base::Vector<Base::Ref<InputGesture>> gestures_;
};

// XAML-authored keyboard binding. The WPF command spelling (for example
// ApplicationCommands.New) is retained while runtime input uses a concrete
// RoutedCommand and KeyGesture.
class AERO_API KeyBinding : public Base::Object {
    AERO_DECLARE_TYPE(KeyBinding, Base::Object)
public:
    KeyBinding() noexcept = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetCommandName() const noexcept { return commandName_.View(); }
    Base::StringView GetKeyName() const noexcept { return keyName_.View(); }
    Base::StringView GetModifiersName() const noexcept { return modifiersName_.View(); }
    Base::Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommandName(Base::StringView value) noexcept;
    void SetKeyName(Base::StringView value) noexcept;
    void SetModifiersName(Base::StringView value) noexcept;
    Base::Result<void> Finalize() noexcept;

private:
    Base::String commandName_;
    Base::String keyName_;
    Base::String modifiersName_;
    Base::Ref<RoutedCommand> command_;
};

class AERO_API CommandBinding {
public:
    CommandBinding() noexcept = default;
    CommandBinding(
        Base::Ref<RoutedCommand> command,
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
    Base::Ref<RoutedCommand> command_;
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

class AERO_API KeyboardNavigation
    : public Base::Object {
    AERO_DECLARE_TYPE(
        KeyboardNavigation,
        Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<KeyboardNavigationMode> DirectionalNavigationProperty{"DirectionalNavigation"};

    inline static constexpr Members::AttachedProperty<KeyboardNavigationMode> TabNavigationProperty{"TabNavigation"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> TabIndexProperty{"TabIndex"};
};


// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.


} // namespace Aero::Input

AERO_DECLARE_TYPE_ENUM(Aero::Input::KeyboardNavigationMode)

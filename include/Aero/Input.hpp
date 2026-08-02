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

namespace Aero::Meta {

template<>
struct TypeTraits<Aero::Input::InputScope> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("InputScope"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "InputScope"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta

#include <Aero/RoutedEvent.hpp>

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

    Base::Result<void> TryAddCanExecuteChanged(
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

class AERO_API KeyGesture final : public InputGesture {
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

class AERO_API RoutedCommand final : public ICommand {
    AERO_DECLARE_TYPE(RoutedCommand, ICommand)
public:
    RoutedCommand() noexcept;
    explicit RoutedCommand(Base::StringView name) noexcept;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetName() const noexcept { return name_.View(); }
    Base::Result<void> TrySetName(Base::StringView name) noexcept;
    Base::Result<void> TryAddInputGesture(
        Base::Ref<InputGesture> gesture) noexcept;
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
    Base::String name_;
    Base::Vector<Base::Ref<InputGesture>> gestures_;
};

// XAML-authored keyboard binding. The WPF command spelling (for example
// ApplicationCommands.New) is retained while runtime input uses a concrete
// RoutedCommand and KeyGesture.
class AERO_API KeyBinding final : public Base::Object {
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

struct CanExecuteRoutedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(CanExecuteRoutedEventArgs, RoutedEventArgs)
public:
    CanExecuteRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(RoutedCommand* value) noexcept { command_ = value; }
    const Meta::Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Meta::Value value) noexcept {
        parameter_ = std::move(value);
    }
    UIElement* GetTarget() const noexcept { return target_; }
    void SetTarget(UIElement* value) noexcept { target_ = value; }
    bool GetCanExecute() const noexcept { return canExecute_; }
    void SetCanExecute(bool value) noexcept { canExecute_ = value; }
    bool GetContinueRouting() const noexcept { return continueRouting_; }
    void SetContinueRouting(bool value) noexcept {
        continueRouting_ = value;
    }

private:
    RoutedCommand* command_ = nullptr;
    Meta::Value parameter_;
    UIElement* target_ = nullptr;
    bool canExecute_ = false;
    bool continueRouting_ = true;
};

struct ExecutedRoutedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(ExecutedRoutedEventArgs, RoutedEventArgs)
public:
    ExecutedRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(RoutedCommand* value) noexcept { command_ = value; }
    const Meta::Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Meta::Value value) noexcept {
        parameter_ = std::move(value);
    }
    UIElement* GetTarget() const noexcept { return target_; }
    void SetTarget(UIElement* value) noexcept { target_ = value; }
    bool GetContinueRouting() const noexcept { return continueRouting_; }
    void SetContinueRouting(bool value) noexcept {
        continueRouting_ = value;
    }

private:
    RoutedCommand* command_ = nullptr;
    Meta::Value parameter_;
    UIElement* target_ = nullptr;
    bool continueRouting_ = true;
};

using CanExecuteRoutedEventHandler = Base::Delegate<void(
    Base::Object*, CanExecuteRoutedEventArgs&)>;
using ExecutedRoutedEventHandler = Base::Delegate<void(
    Base::Object*, ExecutedRoutedEventArgs&)>;

class AERO_API CommandBinding final {
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

class AERO_API KeyboardNavigation final
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

namespace Aero::Meta {

template<>
struct TypeTraits<
    Aero::Input::KeyboardNavigationMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId(
            "KeyboardNavigationMode");
    }
    static constexpr Base::StringView
    Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "KeyboardNavigationMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta

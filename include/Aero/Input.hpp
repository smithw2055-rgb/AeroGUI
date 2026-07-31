#pragma once

#include <Aero/Input/Values.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Visual.hpp>
#include <cstdint>
#include <utility>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Input {

struct KeyboardInput;

using CanExecuteChangedHandler = Base::Delegate<void()>;

class AERO_API ICommand : public Base::Object {
    AERO_DECLARE_TYPE(ICommand, Base::Object)
public:
    ~ICommand() override = default;

    virtual Base::Result<bool> CanExecute(
        const Core::Value& parameter,
        UIElement* target = nullptr) noexcept = 0;
    virtual Base::Result<void> Execute(
        const Core::Value& parameter,
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

    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t Key() const noexcept { return key_; }
    std::uint32_t Modifiers() const noexcept { return modifiers_; }
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

    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Result<void> TrySetName(Base::StringView name) noexcept;
    Base::Result<void> TryAddInputGesture(
        Base::Ref<InputGesture> gesture) noexcept;
    Base::Span<const Base::Ref<InputGesture>> InputGestures() const noexcept {
        return {gestures_.Data(), gestures_.Size()};
    }
    bool MatchesInput(const KeyboardInput& input) const noexcept;

    Base::Result<bool> CanExecute(
        const Core::Value& parameter,
        UIElement* target = nullptr) noexcept override;
    Base::Result<void> Execute(
        const Core::Value& parameter,
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
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView CommandName() const noexcept { return commandName_.View(); }
    Base::StringView KeyName() const noexcept { return keyName_.View(); }
    Base::StringView ModifiersName() const noexcept { return modifiersName_.View(); }
    Base::Ref<RoutedCommand> Command() const noexcept { return command_; }
    Base::Result<void> SetCommandName(Base::StringView value) noexcept;
    Base::Result<void> SetKeyName(Base::StringView value) noexcept;
    Base::Result<void> SetModifiersName(Base::StringView value) noexcept;
    Base::Result<void> Finalize() noexcept;

private:
    Base::String commandName_;
    Base::String keyName_;
    Base::String modifiersName_;
    Base::Ref<RoutedCommand> command_;
};

struct CanExecuteRoutedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(CanExecuteRoutedEventArgs, RoutedEventArgs)
    CanExecuteRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    RoutedCommand* command = nullptr;
    Core::Value parameter;
    UIElement* target = nullptr;
    mutable bool canExecute = false;
    mutable bool continueRouting = true;
};

struct ExecutedRoutedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(ExecutedRoutedEventArgs, RoutedEventArgs)
    ExecutedRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    RoutedCommand* command = nullptr;
    Core::Value parameter;
    UIElement* target = nullptr;
    mutable bool continueRouting = true;
};

using CanExecuteRoutedEventHandler = Base::Delegate<void(
    Base::Object*, const CanExecuteRoutedEventArgs&)>;
using ExecutedRoutedEventHandler = Base::Delegate<void(
    Base::Object*, const ExecutedRoutedEventArgs&)>;

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

    RoutedCommand* Command() const noexcept { return command_.Get(); }
    const Base::Ref<RoutedCommand>& OwnedCommand() const noexcept {
        return command_;
    }
    const CanExecuteRoutedEventHandler& CanExecuteHandler() const noexcept {
        return canExecute_;
    }
    const ExecutedRoutedEventHandler& ExecutedHandler() const noexcept {
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

struct CommandBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

struct InputBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

using RequerySuggestedHandler = Base::Delegate<void()>;


} // namespace Aero::Input

namespace Aero::Input {

using namespace Aero::Core;


// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.

using PointerStateChangedHandler =
    Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler =
    Base::Delegate<void(
        std::uint32_t, UIElement*, bool)>;


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
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        DirectionalNavigationProperty{
            "DirectionalNavigation"};

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        TabNavigationProperty{
            "TabNavigation"};
    inline static constexpr Members::AttachedProperty<
        std::uint32_t>
        TabIndexProperty{"TabIndex"};
};


// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.


} // namespace Aero::Input

namespace Aero::Core {

template<>
struct MetaTypeTraits<
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

} // namespace Aero::Core

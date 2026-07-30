#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Presentation {

class CommandManager;
struct KeyboardInput;

using CanExecuteChangedHandler = Base::Delegate<void()>;

class AERO_API ICommand : public Base::Object {
    AERO_DECLARE_TYPE(ICommand, Base::Object)
public:
    ~ICommand() override = default;

    virtual Base::Result<bool> CanExecute(
        CommandManager& manager,
        const Core::Value& parameter,
        UIElement& target) noexcept = 0;
    virtual Base::Result<void> Execute(
        CommandManager& manager,
        const Core::Value& parameter,
        UIElement& target) noexcept = 0;

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
        CommandManager& manager,
        const Core::Value& parameter,
        UIElement& target) noexcept override;
    Base::Result<void> Execute(
        CommandManager& manager,
        const Core::Value& parameter,
        UIElement& target) noexcept override;

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

class AERO_API CommandManager final {
public:
    explicit CommandManager(ObjectTree& tree) noexcept;

    Base::Result<CommandBindingHandle> TryAddBinding(
        UIElement& owner,
        const CommandBinding& binding) noexcept;
    Base::Result<bool> RemoveBinding(
        CommandBindingHandle handle) noexcept;
    Base::Result<InputBindingHandle> TryAddInputBinding(
        UIElement& owner,
        Base::Ref<KeyBinding> binding) noexcept;

    Base::Result<bool> CanExecute(
        RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> Execute(
        RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> ProcessInput(
        UIElement& target,
        const KeyboardInput& input) noexcept;

    Base::Result<void> TryAddRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    bool RemoveRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    void InvalidateRequerySuggested() const noexcept;

private:
    struct BindingRecord final {
        CommandBindingHandle handle;
        VisualHandle owner;
        CommandBinding binding;
    };
    struct RouteBinding final {
        VisualHandle owner;
        CommandBinding binding;
    };
    struct InputBindingRecord final {
        InputBindingHandle handle;
        VisualHandle owner;
        Base::Ref<KeyBinding> binding;
    };

    ObjectTree* tree_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    Base::Vector<InputBindingRecord> inputBindings_;
    std::uint64_t nextBinding_ = 1U;
    std::uint64_t nextInputBinding_ = 1U;
    RequerySuggestedHandler requerySuggested_;

    Base::Result<void> VerifyTarget(UIElement& target) const noexcept;
    Base::Result<void> SnapshotRoute(
        UIElement& target,
        RoutedCommand* command,
        Base::Vector<RouteBinding>& route) noexcept;
    void PruneStaleBindings() noexcept;
    void PruneStaleInputBindings() noexcept;
};

} // namespace Aero::Presentation

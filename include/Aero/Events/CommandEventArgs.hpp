#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Value.hpp>

#include <utility>

namespace Aero {

namespace Input { class RoutedCommand; }

struct CanExecuteRoutedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(CanExecuteRoutedEventArgs, RoutedEventArgs)
public:
    CanExecuteRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    Input::RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(Input::RoutedCommand* value) noexcept { command_ = value; }
    const Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Value value) noexcept {
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
    Input::RoutedCommand* command_ = nullptr;
    Value parameter_;
    UIElement* target_ = nullptr;
    bool canExecute_ = false;
    bool continueRouting_ = true;
};

struct ExecutedRoutedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(ExecutedRoutedEventArgs, RoutedEventArgs)
public:
    ExecutedRoutedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

    Input::RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(Input::RoutedCommand* value) noexcept { command_ = value; }
    const Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Value value) noexcept {
        parameter_ = std::move(value);
    }
    UIElement* GetTarget() const noexcept { return target_; }
    void SetTarget(UIElement* value) noexcept { target_ = value; }
    bool GetContinueRouting() const noexcept { return continueRouting_; }
    void SetContinueRouting(bool value) noexcept {
        continueRouting_ = value;
    }

private:
    Input::RoutedCommand* command_ = nullptr;
    Value parameter_;
    UIElement* target_ = nullptr;
    bool continueRouting_ = true;
};

using CanExecuteRoutedEventHandler = Base::Delegate<void(
    Base::Object*, CanExecuteRoutedEventArgs&)>;
using ExecutedRoutedEventHandler = Base::Delegate<void(
    Base::Object*, ExecutedRoutedEventArgs&)>;

} // namespace Aero


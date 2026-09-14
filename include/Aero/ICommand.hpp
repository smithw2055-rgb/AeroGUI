#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero { class UIElement; }

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
} // namespace Aero::Input

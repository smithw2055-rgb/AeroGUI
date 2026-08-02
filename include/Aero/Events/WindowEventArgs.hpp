#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Events/EventArgs.hpp>

namespace Aero {

struct CancelEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(CancelEventArgs, RoutedEventArgs)
public:
    CancelEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    bool GetCancel() const noexcept { return cancel_; }
    void SetCancel(bool value) noexcept { cancel_ = value; }

private:
    bool cancel_ = false;
};

using CancelEventHandler = Base::Delegate<void(
    Base::Object*, CancelEventArgs&)>;

} // namespace Aero


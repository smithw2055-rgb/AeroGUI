#pragma once

#include "RuntimeFwd.hpp"
#include <Aero/Controls/Base.hpp>

namespace Aero::Detail {

inline void ControlRuntimeAccess::Attach(
    Controls::Control& control,
    Aero::Detail::RoutedEventManager* events) noexcept {
    control.eventRuntime_ = events;
}

} // namespace Aero::Detail

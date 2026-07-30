#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>

namespace Aero::Detail {

inline void ControlRuntimeAccess::Attach(
    Controls::Control& control,
    Presentation::RoutedEventManager* events) noexcept {
    control.routedEvents_ = events;
}

} // namespace Aero::Detail

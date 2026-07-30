#pragma once

#include <Aero/Controls/ControlPrimitives.hpp>

namespace Aero::Presentation {
class RoutedEventManager;
}

namespace Aero::Detail {

// Private runtime bridge. Routed-event services remain an implementation
// detail while Control-derived types can publish their standard events.
class ControlRuntimeAccess final {
public:
    static void Attach(
        Controls::Control& control,
        Presentation::RoutedEventManager* events) noexcept {
        control.routedEvents_ = events;
    }
};

} // namespace Aero::Detail

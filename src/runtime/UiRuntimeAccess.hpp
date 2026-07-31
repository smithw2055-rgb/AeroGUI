#pragma once

#include "RuntimeFwd.hpp"
#include <Aero/Layout.hpp>

namespace Aero::Detail {

inline void UiRuntimeAccess::SetEventRouter(Aero::UIElement& element, EventRouter* router) noexcept {
    element.eventRouter_ = router;
}

inline void UiRuntimeAccess::SetCommandRouter(
    Aero::UIElement& element,
    CommandManager* manager) noexcept {
    element.commandRouter_ = manager;
}

} // namespace Aero::Detail

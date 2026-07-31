#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Detail {

inline void UiRuntimeAccess::SetCommandRouter(
    Aero::UIElement& element,
    CommandManager* manager) noexcept {
    element.commandRouter_ = manager;
}

} // namespace Aero::Detail

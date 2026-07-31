#pragma once

#include "RuntimeFwd.hpp"
#include <Aero/Controls/Base.hpp>

namespace Aero::Detail {

inline void ControlRuntimeAccess::SetVisualStateManager(Controls::Control& control, Controls::VisualStateManager* visualStates) noexcept {
    control.visualStateRuntime_ = visualStates;
}

} // namespace Aero::Detail

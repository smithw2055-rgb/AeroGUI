#pragma once

// Compatibility shim: property-trigger condition eval lives on TriggerPlan.

#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

inline Base::Result<bool> IsTriggerConditionMet(
    const DependencyObject& object,
    const TriggerPlan& trigger) noexcept {
    return trigger.IsConditionMet(object);
}

} // namespace Aero

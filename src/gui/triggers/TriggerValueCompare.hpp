#pragma once

// Trigger condition value comparison. Mirrors NsGui/Resources/TriggerValueCompare.h
// from the reference repository: pulls the object's current value for a trigger's
// property and compares it against the trigger's comparison value. Used by
// property/Data/Multi/MultiData triggers (binding triggers are evaluated from the
// recorded binding state instead).

#include <Aero/Value.hpp>

#include "gui/core/State.hpp"
#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

inline Base::Result<bool> IsTriggerConditionMet(
    const DependencyObject& object,
    const TriggerPlan& trigger) noexcept {
    Base::Result<PropertyValue> current =
        object.GetValue(trigger.property);
    if (!current) return current.GetStatus();
    return current.Value() == trigger.value;
}

} // namespace Aero

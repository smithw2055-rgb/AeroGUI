#pragma once

#include <Aero/Value.hpp>
#include <Aero/Base/Result.hpp>
#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

namespace Meta {
class Registry;
}

Base::Result<bool> ComparePropertyValues(
    const Meta::PropertyValue& actual,
    Meta::PropertyValue expected,
    const Meta::Registry* metadata = nullptr) noexcept;

inline Base::Result<bool> IsTriggerConditionMet(
    const DependencyObject& object,
    const TriggerPlan& trigger) noexcept {
    return trigger.IsConditionMet(object);
}

} // namespace Aero

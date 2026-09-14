#pragma once

#include <Aero/Value.hpp>
#include <Aero/Base/Result.hpp>
#include "gui/triggers/TriggerPlan.hpp"

namespace Aero {

namespace Meta {
class Registry;
}

enum class PropertyComparisonOperator : std::uint8_t {
    Equal = 0U,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual
};

Base::Result<bool> ComparePropertyValues(
    const Meta::PropertyValue& actual,
    Meta::PropertyValue expected,
    const Meta::Registry* metadata = nullptr,
    PropertyComparisonOperator op = PropertyComparisonOperator::Equal) noexcept;

Base::Result<bool> ComparePropertyValues(
    const Meta::PropertyValue& actual,
    Meta::PropertyValue expected,
    const Meta::Registry* metadata,
    Base::StringView comparison) noexcept;

inline Base::Result<bool> IsTriggerConditionMet(
    const DependencyObject& object,
    const TriggerPlan& trigger) noexcept {
    return trigger.IsConditionMet(object);
}

} // namespace Aero

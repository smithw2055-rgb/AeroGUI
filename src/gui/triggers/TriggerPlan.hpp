#pragma once

// Compiled trigger plan plus shared trigger runtime types/helpers
// (formerly TriggerTypes / TriggerDiagnostics / TriggerValueCompare).

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Data/Binding.hpp>

#include <cstdint>

namespace Aero {

class DependencyObject;
class Style;

namespace Meta {
class Registry;
}

using TriggerActionHandler = Base::Result<void>(*)(
    DependencyObject& owner,
    Base::Span<const Base::Ref<Base::Object>> actions,
    void* context) noexcept;

struct StyleApplication {
    DependencyObject* object = nullptr;
    const Style* style = nullptr;
    Base::Vector<std::uint8_t> triggerStates;
    Base::Vector<std::uint8_t> bindingTriggerStates;
    Base::Vector<std::uint8_t> bindingTriggerKnown;
};

Base::Result<void> InvalidStyle(const char* message) noexcept;

// Returns true when a setter value is a deferred Binding that must be applied
// at load time rather than as a trigger-driven value.
bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept;

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TriggerBindingCondition {
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
};

struct TriggerPlan {
    DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
    Base::Vector<TriggerBindingCondition> extraBindings;
    bool IsBindingTrigger() const noexcept {
        return static_cast<bool>(binding) || !extraBindings.Empty();
    }
    // Property-trigger condition eval lives on the plan; binding triggers use
    // recorded StyleApplication binding state instead.
    Base::Result<bool> IsConditionMet(
        const DependencyObject& object) const noexcept;
    Base::Vector<StyleTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
};

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

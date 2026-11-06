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
#include <Aero/Data/BindingExpression.hpp>

#include <cstdint>

namespace Aero {

class DependencyObject;
class FrameworkElement;
class ResourceDictionary;
class Style;
class TriggerBase;

namespace Meta {
class Registry;
}

using TriggerActionHandler = Base::Result<void>(*)(
    DependencyObject& owner,
    Base::Span<const Base::Ref<Base::Object>> actions,
    void* context) noexcept;

inline constexpr std::uint64_t PropertyTriggerMask(
    DependencyPropertyHandle handle) noexcept {
    if (!handle.IsValid()) return 0ULL;
    const std::uint64_t h = handle.value;
    const std::uint64_t b1 = h & 63ULL;
    const std::uint64_t b2 = ((h >> 6) ^ (h >> 32)) & 63ULL;
    return (1ULL << b1) | (1ULL << b2);
}

struct StyleApplication {
    DependencyObject* object = nullptr;
    const Style* style = nullptr;
    std::uint64_t triggerPropertyMask = 0ULL;
    std::uint64_t triggerStatesMask = 0ULL;
    std::uint64_t bindingTriggerStatesMask = 0ULL;
    std::uint64_t bindingTriggerKnownMask = 0ULL;
    bool triggersInitialized = false;
    Base::Vector<Data::BindingHandle> setterBindings;
};

Base::Result<void> InvalidStyle(const char* message) noexcept;

// Returns true when a setter value is a deferred Binding that must be applied
// at load time rather than as a trigger-driven value.
bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept;
bool IsDynamicResourceSetterValue(
    const PropertyValue& value,
    Base::StringView& key) noexcept;

inline bool ShouldApplyOrClearTriggerSetter(const PropertyValue& value) noexcept {
    Base::StringView dynamicKey;
    return !IsDeferredBindingSetterValue(value) || IsDynamicResourceSetterValue(value, dynamicKey);
}

bool ResolveTriggerSetterValue(
    DependencyObject& target,
    DependencyPropertyHandle property,
    const PropertyValue& rawValue,
    PropertyValue& resolvedValue,
    FrameworkElement* fallbackScope = nullptr,
    const ResourceDictionary* fallbackResources = nullptr,
    bool fallbackToDefaultOnMissingResource = false) noexcept;

// Shared deduplicating bloom + exact-handle filter for Style/Template trigger
// and TemplateBinding property subscriptions (eliminates O(N^2) nested loops).
struct TriggerSubscriptionFilter {
    std::uint64_t mask = 0ULL;
    Base::BasicVector<DependencyPropertyHandle, 16U> handles;

    bool TryAcquire(DependencyPropertyHandle property) noexcept {
        if (!property.IsValid()) return false;
        const std::uint64_t propMask = PropertyTriggerMask(property);
        if ((mask & propMask) == propMask) {
            for (const DependencyPropertyHandle existing : handles) {
                if (existing == property) return false;
            }
        }
        mask |= propMask;
        handles.PushBack(property);
        return true;
    }
};

// Shared inline 64-bit mask + overflow vector for Style and Template trigger evaluation.
struct TriggerActiveBitset {
    std::uint64_t inlineMask = 0ULL;
    Base::Vector<std::uint8_t> overflow;

    explicit TriggerActiveBitset(std::uint32_t count) noexcept {
        if (count > 64U) {
            overflow.Resize(count, 0U);
        }
    }

    void Set(std::uint32_t index, bool active) noexcept {
        if (index < 64U) {
            if (active) {
                inlineMask |= (UINT64_C(1) << index);
            } else {
                inlineMask &= ~(UINT64_C(1) << index);
            }
        } else if (index < overflow.Size()) {
            overflow[index] = active ? 1U : 0U;
        }
    }

    bool Get(std::uint32_t index) const noexcept {
        if (index < 64U) {
            return (inlineMask & (UINT64_C(1) << index)) != 0ULL;
        }
        return index < overflow.Size() && overflow[index] != 0U;
    }
};

struct StyleTriggerSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct CompiledTriggerSetter {
    std::uint32_t target = 0U;
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TriggerBindingCondition {
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
};

struct CompiledTriggerCondition {
    std::int32_t sourceSlot = -1;
    DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    PropertyValue value;
    bool usesDataContext = false;
};

struct CompiledTriggerPlan {
    Base::Ref<Aero::TriggerBase> trigger;
    Base::Vector<CompiledTriggerCondition> conditions;
    Base::Vector<CompiledTriggerSetter> setters;
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
    bool IsConditionMet(
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

bool ComparePropertyValues(
    const Meta::PropertyValue& actual,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata = nullptr,
    PropertyComparisonOperator op = PropertyComparisonOperator::Equal) noexcept;

bool ComparePropertyValues(
    const Meta::PropertyValue& actual,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata,
    Base::StringView comparison) noexcept;

inline bool EvaluatePropertyTriggerCondition(
    const Meta::PropertyValue& current,
    const Meta::PropertyValue& expected,
    const Meta::Registry* metadata = nullptr) noexcept {
    return ComparePropertyValues(current, expected, metadata);
}

inline bool IsTriggerConditionMet(
    const DependencyObject& object,
    const TriggerPlan& trigger) noexcept {
    return trigger.IsConditionMet(object);
}

} // namespace Aero

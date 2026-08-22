#pragma once

// NOTE: The Blend condition primitives (ComparisonCondition,
// ConditionalExpression, ConditionBehavior) were relocated to
// <Aero/Interactivity/Conditions.hpp> in the Aero::Interactivity namespace.

#include <Aero/Data/Binding.hpp>
#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero {

class AERO_GUI_API Condition : public Base::Object {
    AERO_DECLARE_TYPE(Condition, Base::Object)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    void SetPropertyName(StringView value) noexcept;
    StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetSourceName(StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    void SetAuthoredValue(const PropertyValue& value) noexcept {
        if (!value.IsUnset()) authoredValue_ = value;
    }

private:
    Ref<Data::Binding> binding_;
    String propertyName_;
    String sourceName_;
    PropertyValue authoredValue_;
};

} // namespace Aero

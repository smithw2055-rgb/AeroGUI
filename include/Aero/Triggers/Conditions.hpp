#pragma once

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

namespace Aero::Media::Animation {

class AERO_GUI_API ComparisonCondition : public Base::Object {
    AERO_DECLARE_TYPE(ComparisonCondition, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<Aero::Data::Binding> GetLeftOperand() const noexcept { return left_; }
    void SetLeftOperand(Ref<Aero::Data::Binding> value) noexcept {
        left_ = std::move(value);
    }
    const Meta::PropertyValue& GetRightOperand() const noexcept { return right_; }
    void SetRightOperand(const Meta::PropertyValue& value) noexcept { right_ = value; }
    enum class Operator : std::uint8_t {
        Equal = 0U, NotEqual, LessThan, LessThanOrEqual,
        GreaterThan, GreaterThanOrEqual,
    };
    Operator GetComparisonOperator() const noexcept { return operator_; }
    void SetComparisonOperator(Operator value) noexcept { operator_ = value; }

private:
    Ref<Aero::Data::Binding> left_;
    Meta::PropertyValue right_;
    Operator operator_ = Operator::Equal;
};

class AERO_GUI_API ConditionalExpression : public Base::Object {
    AERO_DECLARE_TYPE(ConditionalExpression, Base::Object)
public:
    enum class ForwardChaining : std::uint8_t { And = 0U, Or };
    ConditionalExpression() noexcept : conditions_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Result<void> AddCondition(Ref<ComparisonCondition> value) noexcept {
        return value ? conditions_.PushBack(std::move(value))
                     : Result<void>(Base::Status::Failure(
                           Base::ErrorCode::InvalidArgument, "Condition is null"));
    }
    void ClearConditions() noexcept { conditions_.Clear(); }
    Span<const Ref<ComparisonCondition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    ForwardChaining GetChaining() const noexcept { return chaining_; }
    void SetChaining(ForwardChaining value) noexcept { chaining_ = value; }

private:
    Base::Vector<Ref<ComparisonCondition>> conditions_;
    ForwardChaining chaining_ = ForwardChaining::And;
};

class AERO_GUI_API ConditionBehavior : public Base::Object {
    AERO_DECLARE_TYPE(ConditionBehavior, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Ref<ConditionalExpression> GetExpression() const noexcept { return expression_; }
    void SetExpression(Ref<ConditionalExpression> value) noexcept {
        expression_ = std::move(value);
    }

private:
    Ref<ConditionalExpression> expression_;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ComparisonCondition::Operator)
AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ConditionalExpression::ForwardChaining)

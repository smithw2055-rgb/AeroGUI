#pragma once

#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Triggers/TriggerBase.hpp>

namespace Aero {

class AERO_GUI_API Condition : public Base::Object {
    AERO_DECLARE_TYPE(Condition, Base::Object)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Data::Binding> GetBinding() const noexcept { return binding_; }
    void SetBinding(Base::Ref<Data::Binding> value) noexcept { binding_ = std::move(value); }
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    void SetPropertyName(Base::StringView value) noexcept;
    Base::StringView GetSourceName() const noexcept { return sourceName_.View(); }
    void SetSourceName(Base::StringView value) noexcept;
    const PropertyValue& GetAuthoredValue() const noexcept { return authoredValue_; }
    void SetAuthoredValue(const PropertyValue& value) noexcept {
        if (!value.IsUnset()) authoredValue_ = value;
    }

private:
    Base::Ref<Data::Binding> binding_;
    Base::String propertyName_;
    Base::String sourceName_;
    PropertyValue authoredValue_;
};

} // namespace Aero

namespace Aero::Media::Animation {

class AERO_GUI_API ComparisonCondition : public Base::Object {
    AERO_DECLARE_TYPE(ComparisonCondition, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Aero::Data::Binding> GetLeftOperand() const noexcept { return left_; }
    void SetLeftOperand(Base::Ref<Aero::Data::Binding> value) noexcept {
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
    Base::Ref<Aero::Data::Binding> left_;
    Meta::PropertyValue right_;
    Operator operator_ = Operator::Equal;
};

class AERO_GUI_API ConditionalExpression : public Base::Object {
    AERO_DECLARE_TYPE(ConditionalExpression, Base::Object)
public:
    enum class ForwardChaining : std::uint8_t { And = 0U, Or };
    ConditionalExpression() noexcept : conditions_(&Base::GetDefaultAllocator()) {}
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> AddCondition(Base::Ref<ComparisonCondition> value) noexcept {
        return value ? conditions_.PushBack(std::move(value))
                     : Base::Result<void>(Base::Status::Failure(
                           Base::ErrorCode::InvalidArgument, "Condition is null"));
    }
    void ClearConditions() noexcept { conditions_.Clear(); }
    Base::Span<const Base::Ref<ComparisonCondition>> GetConditions() const noexcept {
        return {conditions_.Data(), conditions_.Size()};
    }
    ForwardChaining GetChaining() const noexcept { return chaining_; }
    void SetChaining(ForwardChaining value) noexcept { chaining_ = value; }

private:
    Base::Vector<Base::Ref<ComparisonCondition>> conditions_;
    ForwardChaining chaining_ = ForwardChaining::And;
};

class AERO_GUI_API ConditionBehavior : public Base::Object {
    AERO_DECLARE_TYPE(ConditionBehavior, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<ConditionalExpression> GetExpression() const noexcept { return expression_; }
    void SetExpression(Base::Ref<ConditionalExpression> value) noexcept {
        expression_ = std::move(value);
    }

private:
    Base::Ref<ConditionalExpression> expression_;
};

} // namespace Aero::Media::Animation

AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ComparisonCondition::Operator)
AERO_DECLARE_TYPE_ENUM(
    Aero::Media::Animation::ConditionalExpression::ForwardChaining)

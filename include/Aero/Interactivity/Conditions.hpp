#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include <Aero/Data/Binding.hpp>
#include <cstdint>

namespace Aero::Interactivity {

// Blend-style condition primitives used by ConditionBehavior. They live in the
// Interactivity namespace (alongside TriggerAction and the behaviors) rather
// than in Media::Animation, since they are authored through interactivity XAML
// and are not tied to the animation timeline model.
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

} // namespace Aero::Interactivity

AERO_DECLARE_TYPE_ENUM(
    Aero::Interactivity::ComparisonCondition::Operator)
AERO_DECLARE_TYPE_ENUM(
    Aero::Interactivity::ConditionalExpression::ForwardChaining)

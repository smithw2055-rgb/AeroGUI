#include "gui/core/EnumRegistration.hpp"

#include <Aero/Interactivity/Conditions.hpp>

namespace Aero {

Base::Result<void> PopulateInteractivityEnums(
    Meta::Registration& context) noexcept {
    using namespace Interactivity;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        ComparisonCondition::Operator,
        "ComparisonConditionOperator",
        description
            .Value("Equal", ComparisonCondition::Operator::Equal)
            .Value("NotEqual", ComparisonCondition::Operator::NotEqual)
            .Value("LessThan", ComparisonCondition::Operator::LessThan)
            .Value("LessThanOrEqual", ComparisonCondition::Operator::LessThanOrEqual)
            .Value("GreaterThan", ComparisonCondition::Operator::GreaterThan)
            .Value("GreaterThanOrEqual", ComparisonCondition::Operator::GreaterThanOrEqual););
    AERO_REGISTER_ENUM(
        ConditionalExpression::ForwardChaining,
        "ForwardChaining",
        description
            .Value("And", ConditionalExpression::ForwardChaining::And)
            .Value("Or", ConditionalExpression::ForwardChaining::Or););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero

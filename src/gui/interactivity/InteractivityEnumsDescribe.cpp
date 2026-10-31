#include "gui/core/EnumRegistration.hpp"

#include <Aero/Interactivity/Conditions.hpp>

namespace Aero {

void PopulateInteractivityEnums(
    Meta::Registration& context) noexcept {
    using namespace Interactivity;

    AERO_AUTO_REGISTER_NAMED_ENUM(ComparisonCondition::Operator, "ComparisonConditionOperator");
    AERO_AUTO_REGISTER_NAMED_ENUM(ConditionalExpression::ForwardChaining, "ForwardChaining");
}

} // namespace Aero

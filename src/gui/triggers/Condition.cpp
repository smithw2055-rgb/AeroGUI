#include <Aero/Style.hpp>
#include <Aero/Triggers/Conditions.hpp>

#include "gui/meta/ValueConversion.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

void Condition::SetPropertyName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    propertyName_ = std::move(candidate);
}

void Condition::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

} // namespace Aero

#include <Aero/Meta/Registration.hpp>

#include "MetaInternals.hpp"

namespace Aero::Core {
namespace {

Detail::MetaRegistrationState& State(void* value) noexcept {
    return *static_cast<Detail::MetaRegistrationState*>(value);
}

} // namespace

RegistrationTypes MetaRegistration::Types() noexcept {
    Detail::MetaRegistrationState& state = State(state_);
    return RegistrationTypes(
        *state.types, *state.behaviors);
}

RegistrationValues MetaRegistration::Values() noexcept {
    return RegistrationValues(
        State(state_).values,
        State(state_).values);
}

RegistrationValues MetaRegistration::Values() const noexcept {
    return RegistrationValues(
        State(state_).values,
        nullptr);
}

ValueTable&
MetaRegistration::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
MetaRegistration::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Core

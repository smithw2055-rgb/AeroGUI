#include <Aero/Meta/Registration.hpp>

#include "MetaInternals.hpp"

namespace Aero::Meta {

using namespace Core;
namespace {

Core::Detail::RegistrationState& State(void* value) noexcept {
    return *static_cast<Core::Detail::RegistrationState*>(value);
}

} // namespace

RegistrationTypes Registration::Types() noexcept {
    Core::Detail::RegistrationState& state = State(state_);
    return RegistrationTypes(
        *state.types, *state.behaviors);
}

RegistrationValues Registration::Values() noexcept {
    return RegistrationValues(
        State(state_).values,
        State(state_).values);
}

RegistrationValues Registration::Values() const noexcept {
    return RegistrationValues(
        State(state_).values,
        nullptr);
}

ValueTable&
Registration::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
Registration::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Meta

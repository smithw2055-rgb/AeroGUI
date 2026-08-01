#include <Aero/Meta/MetadataContext.hpp>

#include "MetadataInternal.hpp"

namespace Aero::Core {
namespace {

Detail::MetadataContextState& State(void* value) noexcept {
    return *static_cast<Detail::MetadataContextState*>(value);
}

} // namespace

MetadataRegistrationTypes MetadataContext::Types() noexcept {
    Detail::MetadataContextState& state = State(state_);
    return MetadataRegistrationTypes(
        *state.types, *state.behaviors);
}

MetadataRegistrationValues MetadataContext::Values() noexcept {
    return MetadataRegistrationValues(
        State(state_).values,
        State(state_).values);
}

MetadataRegistrationValues MetadataContext::Values() const noexcept {
    return MetadataRegistrationValues(
        State(state_).values,
        nullptr);
}

MetadataValueRegistrationStore&
MetadataContext::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
MetadataContext::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Core

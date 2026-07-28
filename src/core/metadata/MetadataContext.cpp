#include <Aero/Core/Metadata/MetadataContext.hpp>

#include "MetadataContextState.hpp"

namespace Aero::Core {
namespace {

Detail::MetadataContextState& State(void* value) noexcept {
    return *static_cast<Detail::MetadataContextState*>(value);
}

const Detail::MetadataContextState& State(
    const void* value) noexcept {
    return *static_cast<const Detail::MetadataContextState*>(value);
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

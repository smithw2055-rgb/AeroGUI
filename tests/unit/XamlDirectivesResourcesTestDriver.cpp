#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace {

Aero::Base::Result<void> SetProbe(
    Aero::Base::Object& object,
    const Aero::Markup::XamlValue& value,
    const Aero::Markup::XamlServiceProvider& services,
    void* context) noexcept;

} // namespace

#include "XamlDirectivesResourcesTests.cpp"

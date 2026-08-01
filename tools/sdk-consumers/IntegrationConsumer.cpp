#include <Aero/Integration.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::View>>
CreateIntegratedView(
    Aero::RuntimeEnvironment& environment,
    Aero::Base::Ref<Aero::Integration::RenderEndpoint>
        endpoint) noexcept {
    Aero::Integration::ViewOptions options;
    options.renderEndpoint = std::move(endpoint);
    return environment.CreateView(options);
}


} // namespace

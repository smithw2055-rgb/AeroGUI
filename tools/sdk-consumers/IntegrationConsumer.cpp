#include <Aero/Integration.hpp>

#include <utility>

namespace {

Aero::Base::Result<Aero::Base::Ref<Aero::View>>
CreateIntegratedView(
    Aero::RuntimeEnvironment& environment,
    Aero::Base::Ref<Aero::Integration::RenderEndpoint>
        endpoint) noexcept {
    Aero::Integration::ViewHostOptions options;
    options.renderEndpoint = std::move(endpoint);
    return Aero::Integration::ViewHost::CreateView(
        environment, options);
}

} // namespace

#include <Aero/Integration.hpp>
#include <Aero/Markup.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::View>>
CreateIntegratedView(
    Aero::Gui& environment,
    Aero::Base::Ref<Aero::Integration::RenderDevice>
        endpoint) noexcept {
    Aero::Integration::ViewOptions options;
    options.renderDevice = std::move(endpoint);
    return environment.CreateView(options);
}

[[maybe_unused]]
void ConsumeViewSurface(Aero::View& view) noexcept {
    Aero::Markup::XamlReader reader(view);
    static_cast<void>(reader.GetView());
    static_cast<void>(view.Update(16U));
}

} // namespace

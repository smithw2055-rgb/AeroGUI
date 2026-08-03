#include <Aero/Integration.hpp>
#include <Aero/Integration/Providers/FontProvider.hpp>
#include <Aero/Integration/Providers/TextureProvider.hpp>
#include <Aero/Integration/Providers/XamlProvider.hpp>
#include <Aero/Markup.hpp>

#include <utility>
#include <type_traits>

namespace {

static_assert(
    std::is_abstract<Aero::Integration::XamlProvider>::value,
    "XamlProvider must remain a host-owned contract");
static_assert(
    std::is_abstract<Aero::Integration::FontProvider>::value,
    "FontProvider must remain a host-owned contract");
static_assert(
    std::is_abstract<Aero::Integration::TextureProvider>::value,
    "TextureProvider must remain a host-owned contract");

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::View>>
CreateIntegratedView(
    Aero::Gui& environment,
    Aero::Base::Ref<Aero::Integration::RenderDevice>
        endpoint) noexcept {
    Aero::Integration::ViewOptions options;
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> created =
        environment.CreateView(options);
    if (!created) return created.GetStatus();
    Aero::Base::Result<void> initialized =
        created.Value()->GetRenderer().Init(std::move(endpoint));
    if (!initialized) return initialized.GetStatus();
    return std::move(created).Value();
}

[[maybe_unused]]
void ConsumeViewSurface(Aero::View& view) noexcept {
    Aero::Markup::XamlReader reader(view);
    static_cast<void>(reader.GetView());
    static_cast<void>(view.Update(16U));
    static_cast<void>(view.GetRenderer().UpdateRenderTree());
    static_cast<void>(view.GetRenderer().RenderOffscreen());
    static_cast<void>(view.GetRenderer().Render());
}

} // namespace

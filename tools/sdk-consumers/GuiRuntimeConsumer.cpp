#include <Aero/Gui.hpp>
#include <Aero/View.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/RenderTarget.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Text/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup.hpp>

#include <type_traits>
#include <utility>

namespace {

static_assert(
    std::is_abstract<Aero::Markup::XamlProvider>::value,
    "XamlProvider must remain a host-owned contract");
static_assert(
    std::is_abstract<Aero::Text::FontProvider>::value,
    "FontProvider must remain a host-owned contract");
static_assert(
    std::is_abstract<Aero::Media::TextureProvider>::value,
    "TextureProvider must remain a host-owned contract");

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::View>>
CreateIntegratedView(
    Aero::Gui& environment,
    Aero::Base::Ref<Aero::RenderTarget> target) noexcept {
    if (!target) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidArgument,
            "Integrated View requires a RenderTarget");
    }
    Aero::ViewOptions options;
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> created =
        environment.CreateView(options);
    if (!created) return created.GetStatus();
    Aero::Base::Result<void> initialized =
        created.Value()->GetRenderer().Init(target->GetDevice());
    if (!initialized) return initialized.GetStatus();
    return std::move(created).Value();
}

[[maybe_unused]]
void ConsumeViewTarget(
    Aero::View& view,
    Aero::RenderTarget& target) noexcept {
    Aero::Markup::XamlReader reader(view.GetGui());
    static_cast<void>(reader.GetGui());
    static_cast<void>(view.Update(16U));
    static_cast<void>(view.GetRenderer().UpdateRenderTree());
    static_cast<void>(view.GetRenderer().RenderOffscreen());
    static_cast<void>(view.GetRenderer().Render(target));
}

} // namespace

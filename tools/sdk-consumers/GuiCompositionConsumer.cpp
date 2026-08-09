#include <Aero/Gui.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/View.hpp>
#include <Aero/ViewOptions.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup/XamlReader.hpp>

#include <type_traits>
#include <utility>

namespace {

static_assert(
    std::is_abstract<Aero::Markup::XamlProvider>::value,
    "XamlProvider must remain a Gui-owned abstract contract");
static_assert(
    std::is_abstract<Aero::Media::FontProvider>::value,
    "FontProvider must remain a Gui-owned abstract contract");
static_assert(
    std::is_abstract<Aero::Media::TextureProvider>::value,
    "TextureProvider must remain a Gui-owned abstract contract");

[[maybe_unused]]
Aero::Result<Aero::Ref<Aero::View>>
CreateIntegratedView(
    Aero::Gui& environment,
    Aero::Ref<Aero::RenderTarget> target) noexcept {
    if (!target) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidArgument,
            "Integrated View requires a RenderTarget");
    }
    Aero::ViewOptions options;
    Aero::Result<Aero::Ref<Aero::View>> created =
        environment.CreateView(options);
    if (!created) return created.GetStatus();
    Aero::Result<void> initialized =
        created.Value()->GetRenderer().Init(target->GetDevice());
    if (!initialized) return initialized.GetStatus();
    return std::move(created).Value();
}

[[maybe_unused]]
Aero::Result<Aero::Ref<Aero::View>>
LoadIntegratedView(Aero::Gui& gui) noexcept {
    Aero::Result<Aero::Ref<Aero::FrameworkElement>> root =
        gui.LoadXaml<Aero::FrameworkElement>("HUD.xaml");
    if (!root) return root.GetStatus();
    return gui.CreateView(std::move(root).Value());
}

[[maybe_unused]]
void ConsumeViewTarget(
    Aero::View& view,
    Aero::RenderTarget& target) noexcept {
    Aero::Markup::XamlReader reader(view.GetGui());
    static_cast<void>(reader.GetGui());
    view.Update(0.0);
    Aero::IRenderer& renderer = view.GetRenderer();
    if (renderer.UpdateRenderTree() && renderer.RenderOffscreen()) {
        renderer.Render(target);
    }
}

} // namespace

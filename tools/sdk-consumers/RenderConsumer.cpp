#include <AeroRender/Render.hpp>

#include <type_traits>

static_assert(
    std::is_base_of<Aero::Base::Object, Aero::RenderDevice>::value,
    "Aero::Render must expose the backend-neutral device contract");
static_assert(
    std::is_base_of<Aero::Base::Object, Aero::RenderTarget>::value,
    "Aero::Render must expose the backend-neutral target contract");
static_assert(
    std::is_destructible<Aero::IRenderer>::value,
    "Aero::Render must expose the View-owned renderer contract");

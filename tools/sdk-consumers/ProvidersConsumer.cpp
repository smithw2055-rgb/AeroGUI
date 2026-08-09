#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Markup/XamlProvider.hpp>

#include <type_traits>

static_assert(std::is_abstract<Aero::Markup::XamlProvider>::value);
static_assert(std::is_abstract<Aero::Text::FontProvider>::value);
static_assert(std::is_abstract<Aero::Media::TextureProvider>::value);
static_assert(sizeof(Aero::Markup::StreamResourceInfo) != 0U);
static_assert(sizeof(Aero::Media::TextureInfo) != 0U);

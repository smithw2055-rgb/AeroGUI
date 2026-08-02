#include <Aero/Integration/Providers/FontProvider.hpp>
#include <Aero/Integration/Providers/TextureProvider.hpp>
#include <Aero/Integration/Providers/XamlProvider.hpp>
#include <Aero/Integration/Providers/Providers.hpp>

#include <type_traits>

static_assert(std::is_abstract<Aero::Integration::XamlProvider>::value);
static_assert(std::is_abstract<Aero::Integration::FontProvider>::value);
static_assert(std::is_abstract<Aero::Integration::TextureProvider>::value);
static_assert(sizeof(Aero::Integration::StreamResourceInfo) != 0U);
static_assert(sizeof(Aero::Integration::TextureInfo) != 0U);

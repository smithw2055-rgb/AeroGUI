#include <AeroAudio/Audio.hpp>

#include <type_traits>

static_assert(
    std::is_default_constructible<Aero::Audio::Engine>::value,
    "Aero::Audio must expose its independent product boundary");

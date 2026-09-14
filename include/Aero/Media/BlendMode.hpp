#pragma once

#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

enum class BlendMode : std::uint8_t {
    Normal = 0U,
    Multiply,
    Screen,
    Additive
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::BlendMode)

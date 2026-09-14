#pragma once

#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

enum class Visibility : std::uint8_t { Visible = 0U, Hidden, Collapsed };

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::Visibility)

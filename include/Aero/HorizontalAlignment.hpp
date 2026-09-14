#pragma once

#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

enum class HorizontalAlignment : std::uint8_t {
    Stretch = 0U,
    Left,
    Center,
    Right
};
enum class VerticalAlignment : std::uint8_t {
    Stretch = 0U,
    Top,
    Center,
    Bottom
};
// Inherited by FrameworkElement so text and templates keep the same logical
// reading direction without every control carrying a duplicate property.
enum class FlowDirection : std::uint8_t { LeftToRight = 0U, RightToLeft };

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::HorizontalAlignment)
AERO_DECLARE_TYPE_ENUM(Aero::VerticalAlignment)
AERO_DECLARE_TYPE_ENUM(Aero::FlowDirection)

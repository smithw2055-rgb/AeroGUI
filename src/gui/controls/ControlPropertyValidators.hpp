#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Layout.hpp>
#include <cmath>

namespace Aero::Controls {

inline bool ValidatePositiveFiniteDouble(const double& value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

inline bool ValidateThicknessValue(const Aero::Thickness& thickness) noexcept {
    return Aero::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

inline bool ValidateNormalizedDouble(const double& value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

inline bool ValidateCornerRadiusValue(const Aero::CornerRadius& radius) noexcept {
    return std::isfinite(radius.topLeft) &&
        std::isfinite(radius.topRight) &&
        std::isfinite(radius.bottomRight) &&
        std::isfinite(radius.bottomLeft) &&
        radius.topLeft >= 0.0 &&
        radius.topRight >= 0.0 &&
        radius.bottomRight >= 0.0 &&
        radius.bottomLeft >= 0.0;
}

} // namespace Aero::Controls

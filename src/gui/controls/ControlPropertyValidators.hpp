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

} // namespace Aero::Controls

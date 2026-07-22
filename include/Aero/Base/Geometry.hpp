#pragma once

#include <Aero/Base/Config.hpp>

#include <cmath>
#include <cstdint>

namespace Aero::Base {

struct Point final { double x = 0.0; double y = 0.0; };
struct Size final { double width = 0.0; double height = 0.0; };
struct Rect final {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};
struct Thickness final {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};
struct Color final {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    float alpha = 1.0F;
};
struct Transform2D final {
    double m11 = 1.0;
    double m12 = 0.0;
    double m21 = 0.0;
    double m22 = 1.0;
    double dx = 0.0;
    double dy = 0.0;
};

using RenderNodeId = std::uint64_t;
constexpr RenderNodeId InvalidRenderNodeId = 0U;

AERO_NODISCARD inline bool IsFiniteRect(Rect value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.width) && std::isfinite(value.height);
}

AERO_NODISCARD inline bool IsFiniteColor(Color value) noexcept {
    return std::isfinite(value.red) && std::isfinite(value.green) &&
        std::isfinite(value.blue) && std::isfinite(value.alpha);
}

AERO_NODISCARD inline bool IsFiniteTransform(Transform2D value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12) &&
        std::isfinite(value.m21) && std::isfinite(value.m22) &&
        std::isfinite(value.dx) && std::isfinite(value.dy);
}

AERO_NODISCARD inline bool IsValidRect(Rect value) noexcept {
    return IsFiniteRect(value) && value.width >= 0.0 && value.height >= 0.0;
}

AERO_NODISCARD inline bool IsNormalizedOpacity(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

} // namespace Aero::Base

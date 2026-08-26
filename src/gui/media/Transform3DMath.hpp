#pragma once

// Internal 2.5D helpers shared by render and (Wave 2) local visual transform.
// ApproximateCompositeProjection is the pre-projective 2D formula; Wave 3
// render collapse stops calling it.

#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/Media/Transforms.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Media {

inline Base::Transform2D ApproximateCompositeProjection(
    const CompositeTransform3D& value) noexcept {
    constexpr double Pi = 3.1415926535897932384626433832795;
    constexpr double Perspective = Base::DefaultPerspectiveDepth;
    const double radiansX = value.GetRotationX() * Pi / 180.0;
    const double radiansY = value.GetRotationY() * Pi / 180.0;
    const double radiansZ = value.GetRotationZ() * Pi / 180.0;
    const double depth = std::max(
        -Perspective * Base::PerspectiveZClampFraction,
        std::min(
            Perspective * Base::PerspectiveZClampFraction,
            value.GetTranslateZ() + value.GetCenterZ()));
    const double perspective = Perspective / (Perspective - depth);
    const double scaleX = value.GetScaleX() * std::cos(radiansY) * perspective;
    const double scaleY = value.GetScaleY() * std::cos(radiansX) * perspective;
    const double cosine = std::cos(radiansZ);
    const double sine = std::sin(radiansZ);
    Base::Transform2D matrix;
    matrix.m11 = scaleX * cosine;
    matrix.m12 = scaleX * sine;
    matrix.m21 = -scaleY * sine;
    matrix.m22 = scaleY * cosine;
    matrix.dx = value.GetTranslateX();
    matrix.dy = value.GetTranslateY();
    Base::Transform2D before;
    before.dx = -value.GetCenterX();
    before.dy = -value.GetCenterY();
    Base::Transform2D after;
    after.dx = value.GetCenterX();
    after.dy = value.GetCenterY();
    return ComposeTransforms(ComposeTransforms(before, matrix), after);
}

} // namespace Aero::Media

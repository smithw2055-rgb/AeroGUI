#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

Result<void> FlattenCubicBezier(
    FlattenSink& sink,
    Point start,
    Point control1,
    Point control2,
    Point end) noexcept;
Result<void> FlattenQuadraticBezier(
    FlattenSink& sink,
    Point start,
    Point control,
    Point end) noexcept;
Result<void> FlattenArc(
    FlattenSink& sink,
    Point start,
    Size radii,
    double rotationDegrees,
    bool isLargeArc,
    bool sweepClockwise,
    Point end) noexcept;
Result<void> ParsePointList(
    StringView text,
    Base::Vector<Point>& points) noexcept;

} // namespace Aero::Media

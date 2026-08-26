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

// Scanline fill of Geometry.Flatten contours. EvenOdd. Used by Path
// tessellation reuse for UIElement.Clip stencil meshes.
Result<void> TessellateGeometryFill(
    const Geometry& geometry,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept;

bool GeometryContainsLocalPoint(
    const Geometry& geometry,
    Point point) noexcept;

} // namespace Aero::Media

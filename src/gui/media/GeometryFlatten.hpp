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

// Flatten SVG/WPF mini-language into BeginFigure/AddPoint/EndFigure.
Result<void> FlattenPathData(
    StringView data,
    FlattenSink& sink) noexcept;

// Scanline fill of Geometry.Flatten contours. EvenOdd. Used by Path
// tessellation reuse for UIElement.Clip stencil meshes.
Result<void> TessellateGeometryFill(
    const Geometry& geometry,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept;

// P4.4: single-flatten shared contour set. FillContour is the scanline
// unit; the stroke side reuses the same flattened points with its own
// (start, count, closed) index. FlattenGeometryContours runs one
// geometry.Flatten feeding both representations so DrawGeometry(fill+pen)
// no longer pays the subdivision twice.
struct FillContour {
    std::uint32_t offset = 0U;
    std::uint32_t count = 0U;
};

// Phase A: flatten only.
Result<void> FlattenGeometryContours(
    const Geometry& geometry,
    Base::Vector<Point>& fillPoints,
    Base::Vector<FillContour>& fillContours,
    Base::Vector<Point>& strokePoints,
    Base::Vector<std::uint32_t>& strokeStarts,
    Base::Vector<std::uint32_t>& strokeCounts,
    Base::Vector<std::uint8_t>& strokeClosed) noexcept;

// Phase B: scanline fill from pre-flattened contours.
Result<void> TessellateFillContours(
    const Base::Vector<Point>& points,
    const Base::Vector<FillContour>& contours,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept;

bool GeometryContainsLocalPoint(
    const Geometry& geometry,
    Point point) noexcept;

} // namespace Aero::Media

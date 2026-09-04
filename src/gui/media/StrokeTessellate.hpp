#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/Pen.hpp>

namespace Aero::Media {

Result<void> ExpandDashPattern(
    Base::Span<const double> dashes,
    Base::Vector<double>& pattern) noexcept;

Result<void> TessellateStroke(
    const Base::Vector<Point>& points,
    const Base::Vector<std::uint32_t>& contourStarts,
    const Base::Vector<std::uint32_t>& contourCounts,
    const Base::Vector<std::uint8_t>& contourClosed,
    double thickness,
    double trimStart,
    double trimEnd,
    PenLineJoin join,
    PenLineCap startCap,
    PenLineCap endCap,
    double miterLimit,
    Base::Span<const double> dashes,
    double dashOffset,
    Base::Vector<Point>& vertices,
    Base::Vector<std::uint32_t>& indices) noexcept;

} // namespace Aero::Media

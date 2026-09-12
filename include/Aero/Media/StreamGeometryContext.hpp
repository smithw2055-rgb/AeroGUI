#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Media/ArcSegment.hpp>

#include <cstdint>

namespace Aero::Media {

class StreamGeometry;

// Programmatic StreamGeometry writer. Records figure commands without
// allocating PathSegment objects or parsing mini-language text.
class AERO_GUI_API StreamGeometryContext {
public:
    StreamGeometryContext() noexcept = default;
    StreamGeometryContext(const StreamGeometryContext&) = delete;
    StreamGeometryContext& operator=(const StreamGeometryContext&) = delete;
    StreamGeometryContext(StreamGeometryContext&& other) noexcept;
    StreamGeometryContext& operator=(StreamGeometryContext&& other) noexcept;
    ~StreamGeometryContext();

    Result<void> BeginFigure(
        Point startPoint,
        bool isFilled,
        bool isClosed) noexcept;
    Result<void> LineTo(
        Point point,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    Result<void> BezierTo(
        Point controlPoint1,
        Point controlPoint2,
        Point endPoint,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    Result<void> QuadraticBezierTo(
        Point controlPoint,
        Point endPoint,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    Result<void> ArcTo(
        Point point,
        Size size,
        double rotationAngle,
        bool isLargeArc,
        SweepDirection sweepDirection,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    Result<void> PolyLineTo(
        const Point* points,
        std::uint32_t count,
        bool isStroked,
        bool isSmoothJoin) noexcept;
    Result<void> Close() noexcept;

private:
    friend class StreamGeometry;
    explicit StreamGeometryContext(StreamGeometry* owner) noexcept;
    StreamGeometry* owner_ = nullptr;
    bool closed_ = true;
};

} // namespace Aero::Media

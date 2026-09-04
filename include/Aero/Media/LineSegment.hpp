#pragma once

#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

class AERO_GUI_API LineSegment : public PathSegment {
    AERO_DECLARE_TYPE(LineSegment, PathSegment)
public:
    LineSegment() noexcept : PathSegment(StaticTypeId()) {}
    Point GetPoint() const noexcept {
        return GetValue(PointProperty);
    }
    void SetPoint(Point value) noexcept {
        SetValue(PointProperty, value);
    }
    inline static constexpr DependencyProperty<Point> PointProperty{"Point"};
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
};
} // namespace Aero::Media

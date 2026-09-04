#pragma once

#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

class AERO_GUI_API QuadraticBezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(QuadraticBezierSegment, PathSegment)
public:
    QuadraticBezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    Point GetPoint1() const noexcept {
        return GetValue(Point1Property);
    }
    Point GetPoint2() const noexcept {
        return GetValue(Point2Property);
    }
    void SetPoint1(Point value) noexcept { SetValue(Point1Property, value); }
    void SetPoint2(Point value) noexcept { SetValue(Point2Property, value); }
    inline static constexpr DependencyProperty<Point> Point1Property{"Point1"};
    inline static constexpr DependencyProperty<Point> Point2Property{"Point2"};
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
};
} // namespace Aero::Media

#pragma once

#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

class AERO_GUI_API BezierSegment : public PathSegment {
    AERO_DECLARE_TYPE(BezierSegment, PathSegment)
public:
    BezierSegment() noexcept : PathSegment(StaticTypeId()) {}
    Point GetPoint1() const noexcept {
        return GetValue(Point1Property);
    }
    Point GetPoint2() const noexcept {
        return GetValue(Point2Property);
    }
    Point GetPoint3() const noexcept {
        return GetValue(Point3Property);
    }
    void SetPoint1(Point value) noexcept { SetValue(Point1Property, value); }
    void SetPoint2(Point value) noexcept { SetValue(Point2Property, value); }
    void SetPoint3(Point value) noexcept { SetValue(Point3Property, value); }
    inline static constexpr DependencyProperty<Point> Point1Property{"Point1"};
    inline static constexpr DependencyProperty<Point> Point2Property{"Point2"};
    inline static constexpr DependencyProperty<Point> Point3Property{"Point3"};
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
};
} // namespace Aero::Media

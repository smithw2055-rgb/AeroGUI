#pragma once

#include <Aero/Media/PathSegment.hpp>

#include <cstdint>

namespace Aero::Media {

enum class SweepDirection : std::uint8_t {
    Counterclockwise = 0U,
    Clockwise
};

class AERO_GUI_API ArcSegment : public PathSegment {
    AERO_DECLARE_TYPE(ArcSegment, PathSegment)
public:
    ArcSegment() noexcept : PathSegment(StaticTypeId()) {}
    Point GetPoint() const noexcept {
        return GetValue(PointProperty);
    }
    Size GetSize() const noexcept {
        return GetValue(SizeProperty);
    }
    double GetRotationAngle() const noexcept {
        return GetValue(RotationAngleProperty);
    }
    bool GetIsLargeArc() const noexcept {
        return GetValue(IsLargeArcProperty);
    }
    SweepDirection GetSweepDirection() const noexcept {
        return GetValue(SweepDirectionProperty);
    }
    void SetPoint(Point value) noexcept { SetValue(PointProperty, value); }
    void SetSize(Size value) noexcept { SetValue(SizeProperty, value); }
    void SetRotationAngle(double value) noexcept {
        SetValue(RotationAngleProperty, value);
    }
    void SetIsLargeArc(bool value) noexcept {
        SetValue(IsLargeArcProperty, value);
    }
    void SetSweepDirection(SweepDirection value) noexcept {
        SetValue(SweepDirectionProperty, value);
    }
    AERO_DEPENDENCY_PROPERTY(Point, Point);
    AERO_DEPENDENCY_PROPERTY(Size, Size);
    AERO_DEPENDENCY_PROPERTY(double, RotationAngle);
    AERO_DEPENDENCY_PROPERTY(bool, IsLargeArc);
    AERO_DEPENDENCY_PROPERTY(SweepDirection, SweepDirection);
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
};
} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::SweepDirection)

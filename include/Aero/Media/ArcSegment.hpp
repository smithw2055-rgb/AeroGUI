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
        return GetValueOr(PointProperty, Point{});
    }
    Size GetSize() const noexcept {
        return GetValueOr(SizeProperty, Size{});
    }
    double GetRotationAngle() const noexcept {
        return GetValueOr(RotationAngleProperty, 0.0);
    }
    bool GetIsLargeArc() const noexcept {
        return GetValueOr(IsLargeArcProperty, false);
    }
    SweepDirection GetSweepDirection() const noexcept {
        return GetValueOr(
            SweepDirectionProperty, SweepDirection::Counterclockwise);
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
    inline static constexpr DependencyProperty<Point> PointProperty{"Point"};
    inline static constexpr DependencyProperty<Size> SizeProperty{"Size"};
    inline static constexpr DependencyProperty<double> RotationAngleProperty{"RotationAngle"};
    inline static constexpr DependencyProperty<bool> IsLargeArcProperty{"IsLargeArc"};
    inline static constexpr DependencyProperty<SweepDirection> SweepDirectionProperty{"SweepDirection"};
    Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept override;
};
} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::SweepDirection)

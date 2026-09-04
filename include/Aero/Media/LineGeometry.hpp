#pragma once

#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API LineGeometry : public Geometry {
    AERO_DECLARE_TYPE(LineGeometry, Geometry)
public:
    LineGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Point GetStartPoint() const noexcept {
        return GetValue(StartPointProperty);
    }
    Point GetEndPoint() const noexcept {
        return GetValue(EndPointProperty);
    }
    void SetStartPoint(Point value) noexcept {
        SetValue(StartPointProperty, value);
    }
    void SetEndPoint(Point value) noexcept {
        SetValue(EndPointProperty, value);
    }
    Rect GetBounds() const noexcept override;
    inline static constexpr DependencyProperty<Point> StartPointProperty{"StartPoint"};
    inline static constexpr DependencyProperty<Point> EndPointProperty{"EndPoint"};
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
};
} // namespace Aero::Media

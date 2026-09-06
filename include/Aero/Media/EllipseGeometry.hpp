#pragma once

#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API EllipseGeometry : public Geometry {
    AERO_DECLARE_TYPE(EllipseGeometry, Geometry)
public:
    EllipseGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Point GetCenter() const noexcept {
        return GetValue(CenterProperty);
    }
    double GetRadiusX() const noexcept {
        return GetValue(RadiusXProperty);
    }
    double GetRadiusY() const noexcept {
        return GetValue(RadiusYProperty);
    }
    void SetCenter(Point value) noexcept { SetValue(CenterProperty, value); }
    void SetRadiusX(double value) noexcept { SetValue(RadiusXProperty, value); }
    void SetRadiusY(double value) noexcept { SetValue(RadiusYProperty, value); }
    Rect GetBounds() const noexcept override;
    AERO_DEPENDENCY_PROPERTY(Point, Center);
    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
};
} // namespace Aero::Media

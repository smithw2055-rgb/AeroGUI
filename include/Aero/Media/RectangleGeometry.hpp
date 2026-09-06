#pragma once

#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API RectangleGeometry : public Geometry {
    AERO_DECLARE_TYPE(RectangleGeometry, Geometry)
public:
    RectangleGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Rect GetRect() const noexcept {
        return GetValue(RectProperty);
    }
    double GetRadiusX() const noexcept {
        return GetValue(RadiusXProperty);
    }
    double GetRadiusY() const noexcept {
        return GetValue(RadiusYProperty);
    }
    void SetRect(Rect value) noexcept { SetValue(RectProperty, value); }
    void SetRadiusX(double value) noexcept { SetValue(RadiusXProperty, value); }
    void SetRadiusY(double value) noexcept { SetValue(RadiusYProperty, value); }
    Rect GetBounds() const noexcept override { return GetRect(); }
    AERO_DEPENDENCY_PROPERTY(Rect, Rect);
    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
};
} // namespace Aero::Media

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
        return GetValueOr(RectProperty, Rect{});
    }
    double GetRadiusX() const noexcept {
        return GetValueOr(RadiusXProperty, 0.0);
    }
    double GetRadiusY() const noexcept {
        return GetValueOr(RadiusYProperty, 0.0);
    }
    void SetRect(Rect value) noexcept { SetValue(RectProperty, value); }
    void SetRadiusX(double value) noexcept { SetValue(RadiusXProperty, value); }
    void SetRadiusY(double value) noexcept { SetValue(RadiusYProperty, value); }
    Rect GetBounds() const noexcept override { return GetRect(); }
    inline static constexpr DependencyProperty<Rect> RectProperty{"Rect"};
    inline static constexpr DependencyProperty<double> RadiusXProperty{"RadiusX"};
    inline static constexpr DependencyProperty<double> RadiusYProperty{"RadiusY"};
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
};
} // namespace Aero::Media

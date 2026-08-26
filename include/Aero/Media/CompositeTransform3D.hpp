#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API CompositeTransform3D : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(CompositeTransform3D, ::Aero::DependencyObject)
public:
    CompositeTransform3D() noexcept : DependencyObject(StaticTypeId()) {}

    double GetCenterX() const noexcept { return GetValueOr(CenterXProperty, 0.0); }
    double GetCenterY() const noexcept { return GetValueOr(CenterYProperty, 0.0); }
    double GetCenterZ() const noexcept { return GetValueOr(CenterZProperty, 0.0); }
    double GetRotationX() const noexcept { return GetValueOr(RotationXProperty, 0.0); }
    double GetRotationY() const noexcept { return GetValueOr(RotationYProperty, 0.0); }
    double GetRotationZ() const noexcept { return GetValueOr(RotationZProperty, 0.0); }
    double GetScaleX() const noexcept { return GetValueOr(ScaleXProperty, 1.0); }
    double GetScaleY() const noexcept { return GetValueOr(ScaleYProperty, 1.0); }
    double GetScaleZ() const noexcept { return GetValueOr(ScaleZProperty, 1.0); }
    double GetTranslateX() const noexcept { return GetValueOr(TranslateXProperty, 0.0); }
    double GetTranslateY() const noexcept { return GetValueOr(TranslateYProperty, 0.0); }
    double GetTranslateZ() const noexcept { return GetValueOr(TranslateZProperty, 0.0); }

    void SetCenterX(double value) noexcept { SetValue(CenterXProperty, value); }
    void SetCenterY(double value) noexcept { SetValue(CenterYProperty, value); }
    void SetCenterZ(double value) noexcept { SetValue(CenterZProperty, value); }
    void SetRotationX(double value) noexcept { SetValue(RotationXProperty, value); }
    void SetRotationY(double value) noexcept { SetValue(RotationYProperty, value); }
    void SetRotationZ(double value) noexcept { SetValue(RotationZProperty, value); }
    void SetScaleX(double value) noexcept { SetValue(ScaleXProperty, value); }
    void SetScaleY(double value) noexcept { SetValue(ScaleYProperty, value); }
    void SetScaleZ(double value) noexcept { SetValue(ScaleZProperty, value); }
    void SetTranslateX(double value) noexcept { SetValue(TranslateXProperty, value); }
    void SetTranslateY(double value) noexcept { SetValue(TranslateYProperty, value); }
    void SetTranslateZ(double value) noexcept { SetValue(TranslateZProperty, value); }

    Base::Transform2D GetProjectedMatrix() const noexcept;

    inline static constexpr DependencyProperty<double> CenterXProperty{"CenterX"};
    inline static constexpr DependencyProperty<double> CenterYProperty{"CenterY"};
    inline static constexpr DependencyProperty<double> CenterZProperty{"CenterZ"};
    inline static constexpr DependencyProperty<double> RotationXProperty{"RotationX"};
    inline static constexpr DependencyProperty<double> RotationYProperty{"RotationY"};
    inline static constexpr DependencyProperty<double> RotationZProperty{"RotationZ"};
    inline static constexpr DependencyProperty<double> ScaleXProperty{"ScaleX"};
    inline static constexpr DependencyProperty<double> ScaleYProperty{"ScaleY"};
    inline static constexpr DependencyProperty<double> ScaleZProperty{"ScaleZ"};
    inline static constexpr DependencyProperty<double> TranslateXProperty{"TranslateX"};
    inline static constexpr DependencyProperty<double> TranslateYProperty{"TranslateY"};
    inline static constexpr DependencyProperty<double> TranslateZProperty{"TranslateZ"};
};
} // namespace Aero::Media

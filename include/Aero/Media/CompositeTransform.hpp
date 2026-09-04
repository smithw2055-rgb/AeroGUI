#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

/// 2D composite: Center + Scale / Skew / Rotate / Translate.
/// GetMatrix() composes the existing 2D transform primitives around Center.
class AERO_GUI_API CompositeTransform : public Transform {
    AERO_DECLARE_TYPE(CompositeTransform, Transform)
public:
    CompositeTransform() noexcept : Transform(StaticTypeId()) {}

    double GetCenterX() const noexcept { return GetValue(CenterXProperty); }
    double GetCenterY() const noexcept { return GetValue(CenterYProperty); }
    double GetScaleX() const noexcept { return GetValue(ScaleXProperty); }
    double GetScaleY() const noexcept { return GetValue(ScaleYProperty); }
    double GetSkewX() const noexcept { return GetValue(SkewXProperty); }
    double GetSkewY() const noexcept { return GetValue(SkewYProperty); }
    double GetRotation() const noexcept { return GetValue(RotationProperty); }
    double GetTranslateX() const noexcept { return GetValue(TranslateXProperty); }
    double GetTranslateY() const noexcept { return GetValue(TranslateYProperty); }

    void SetCenterX(double value) noexcept { SetValue(CenterXProperty, value); }
    void SetCenterY(double value) noexcept { SetValue(CenterYProperty, value); }
    void SetScaleX(double value) noexcept { SetValue(ScaleXProperty, value); }
    void SetScaleY(double value) noexcept { SetValue(ScaleYProperty, value); }
    void SetSkewX(double value) noexcept { SetValue(SkewXProperty, value); }
    void SetSkewY(double value) noexcept { SetValue(SkewYProperty, value); }
    void SetRotation(double value) noexcept { SetValue(RotationProperty, value); }
    void SetTranslateX(double value) noexcept { SetValue(TranslateXProperty, value); }
    void SetTranslateY(double value) noexcept { SetValue(TranslateYProperty, value); }

    Base::Transform2D GetMatrix() const noexcept override;

    inline static constexpr DependencyProperty<double> CenterXProperty{"CenterX"};
    inline static constexpr DependencyProperty<double> CenterYProperty{"CenterY"};
    inline static constexpr DependencyProperty<double> ScaleXProperty{"ScaleX"};
    inline static constexpr DependencyProperty<double> ScaleYProperty{"ScaleY"};
    inline static constexpr DependencyProperty<double> SkewXProperty{"SkewX"};
    inline static constexpr DependencyProperty<double> SkewYProperty{"SkewY"};
    inline static constexpr DependencyProperty<double> RotationProperty{"Rotation"};
    inline static constexpr DependencyProperty<double> TranslateXProperty{"TranslateX"};
    inline static constexpr DependencyProperty<double> TranslateYProperty{"TranslateY"};
};

} // namespace Aero::Media

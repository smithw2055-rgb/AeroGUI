#pragma once

#include <Aero/Media/Transform3D.hpp>

namespace Aero::Media {

/// Convenience 3D transform: Center / Scale / Rotate / Translate.
/// GetTransform3D() returns the 4×3 affine; perspective collapse happens at
/// render/hit via Transform3DContext (not here).
class AERO_GUI_API CompositeTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(CompositeTransform3D, Transform3D)
public:
    CompositeTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    double GetCenterX() const noexcept { return GetValue(CenterXProperty); }
    double GetCenterY() const noexcept { return GetValue(CenterYProperty); }
    double GetCenterZ() const noexcept { return GetValue(CenterZProperty); }
    double GetRotationX() const noexcept { return GetValue(RotationXProperty); }
    double GetRotationY() const noexcept { return GetValue(RotationYProperty); }
    double GetRotationZ() const noexcept { return GetValue(RotationZProperty); }
    double GetScaleX() const noexcept { return GetValue(ScaleXProperty); }
    double GetScaleY() const noexcept { return GetValue(ScaleYProperty); }
    double GetScaleZ() const noexcept { return GetValue(ScaleZProperty); }
    double GetTranslateX() const noexcept { return GetValue(TranslateXProperty); }
    double GetTranslateY() const noexcept { return GetValue(TranslateYProperty); }
    double GetTranslateZ() const noexcept { return GetValue(TranslateZProperty); }

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

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);
    AERO_DEPENDENCY_PROPERTY(double, CenterZ);
    AERO_DEPENDENCY_PROPERTY(double, RotationX);
    AERO_DEPENDENCY_PROPERTY(double, RotationY);
    AERO_DEPENDENCY_PROPERTY(double, RotationZ);
    AERO_DEPENDENCY_PROPERTY(double, ScaleX);
    AERO_DEPENDENCY_PROPERTY(double, ScaleY);
    AERO_DEPENDENCY_PROPERTY(double, ScaleZ);
    AERO_DEPENDENCY_PROPERTY(double, TranslateX);
    AERO_DEPENDENCY_PROPERTY(double, TranslateY);
    AERO_DEPENDENCY_PROPERTY(double, TranslateZ);
};

} // namespace Aero::Media

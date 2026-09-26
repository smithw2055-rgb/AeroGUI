#pragma once

// 3D transform family, including the Transform3D base.
// XAML type names stay Transform3D, PerspectiveTransform3D, ...
#include <Aero/Base/Geometry.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

/// 3D affine transform applied to a visual (UWP-style Transform3D).
/// Direct Freezable subclass — no Animatable layer.
class AERO_GUI_API Transform3D : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Transform3D, ::Aero::Freezable)
public:
    ~Transform3D() override = default;

    /// Local 3D affine (identity for camera-like PerspectiveTransform3D).
    [[nodiscard]] virtual Base::Transform3 GetTransform3D() const noexcept = 0;

protected:
    explicit Transform3D(Meta::TypeId runtimeType) noexcept : Freezable(runtimeType) {}
};

/// Tree-walk state shared by render collapse and hit-test unproject.
/// Not a public WPF type. `active` is true when a PerspectiveTransform3D
/// ancestor (or the implicit view-root default Depth) is in effect.
struct Transform3DContext {
    bool active = false;
    Base::Transform3 accumulated = Base::IdentityTransform3();
    double depth = 0.0;
    Base::Point offset{};
    /// Collapse center in the perspective-root's local pixels
    /// (`renderSize/2 + offset`). Shared vanishing point for the subtree.
    Base::Point center{};
};

/// Camera-like perspective for descendants. Depth/Offset are consumed by
/// collapse, not by GetTransform3D() (which is identity). Inherited along the
/// visual tree via Transform3DContext — not via DP Inherits().
class AERO_GUI_API PerspectiveTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(PerspectiveTransform3D, Transform3D)
public:
    PerspectiveTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    double GetDepth() const noexcept { return GetValue(DepthProperty); }
    double GetOffsetX() const noexcept { return GetValue(OffsetXProperty); }
    double GetOffsetY() const noexcept { return GetValue(OffsetYProperty); }

    void SetDepth(double value) noexcept { SetValue(DepthProperty, value); }
    void SetOffsetX(double value) noexcept { SetValue(OffsetXProperty, value); }
    void SetOffsetY(double value) noexcept { SetValue(OffsetYProperty, value); }

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY(double, Depth);
    AERO_DEPENDENCY_PROPERTY(double, OffsetX);
    AERO_DEPENDENCY_PROPERTY(double, OffsetY);
};

/// Arbitrary 4×3 affine 3D transform. MatrixProperty is fully registered in Meta
/// (unlike some reference implementations that leave it unregistered).
class AERO_GUI_API MatrixTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(MatrixTransform3D, Transform3D)
public:
    MatrixTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    Base::Transform3 GetMatrix() const noexcept { return GetValue(MatrixProperty); }
    void SetMatrix(Base::Transform3 value) noexcept { SetValue(MatrixProperty, value); }

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY(Base::Transform3, Matrix);
};

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

AERO_DECLARE_TYPE_VALUE(Base::Transform3, "Transform3")

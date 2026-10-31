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

    std::uint64_t GetRevision() const noexcept { return Revision(); }

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

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY2(double, Depth);
    AERO_DEPENDENCY_PROPERTY2(double, OffsetX);
    AERO_DEPENDENCY_PROPERTY2(double, OffsetY);
};

/// Arbitrary 4×3 affine 3D transform. MatrixProperty is fully registered in Meta
/// (unlike some reference implementations that leave it unregistered).
class AERO_GUI_API MatrixTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(MatrixTransform3D, Transform3D)

public:
    MatrixTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY2(Base::Transform3, Matrix);
};

/// Convenience 3D transform: Center / Scale / Rotate / Translate.
/// GetTransform3D() returns the 4×3 affine; perspective collapse happens at
/// render/hit via Transform3DContext (not here).
class AERO_GUI_API CompositeTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(CompositeTransform3D, Transform3D)

public:
    CompositeTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    AERO_DEPENDENCY_PROPERTY2(double, CenterX);
    AERO_DEPENDENCY_PROPERTY2(double, CenterY);
    AERO_DEPENDENCY_PROPERTY2(double, CenterZ);
    AERO_DEPENDENCY_PROPERTY2(double, RotationX);
    AERO_DEPENDENCY_PROPERTY2(double, RotationY);
    AERO_DEPENDENCY_PROPERTY2(double, RotationZ);
    AERO_DEPENDENCY_PROPERTY2(double, ScaleX);
    AERO_DEPENDENCY_PROPERTY2(double, ScaleY);
    AERO_DEPENDENCY_PROPERTY2(double, ScaleZ);
    AERO_DEPENDENCY_PROPERTY2(double, TranslateX);
    AERO_DEPENDENCY_PROPERTY2(double, TranslateY);
    AERO_DEPENDENCY_PROPERTY2(double, TranslateZ);
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_VALUE(Base::Transform3, "Transform3")

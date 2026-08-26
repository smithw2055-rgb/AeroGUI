#pragma once

#include <Aero/Media/Transform3D.hpp>

namespace Aero::Media {

/// Arbitrary 4×3 affine 3D transform. MatrixProperty is fully registered in Meta
/// (unlike some reference implementations that leave it unregistered).
class AERO_GUI_API MatrixTransform3D : public Transform3D {
    AERO_DECLARE_TYPE(MatrixTransform3D, Transform3D)
public:
    MatrixTransform3D() noexcept : Transform3D(StaticTypeId()) {}

    Base::Transform3 GetMatrix() const noexcept {
        return GetValueOr(MatrixProperty, Base::IdentityTransform3());
    }
    void SetMatrix(Base::Transform3 value) noexcept {
        SetValue(MatrixProperty, value);
    }

    [[nodiscard]] Base::Transform3 GetTransform3D() const noexcept override;

    inline static constexpr DependencyProperty<Base::Transform3> MatrixProperty{"Matrix"};
};

} // namespace Aero::Media

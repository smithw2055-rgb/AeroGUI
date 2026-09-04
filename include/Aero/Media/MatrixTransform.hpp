#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API MatrixTransform : public Transform {
    AERO_DECLARE_TYPE(MatrixTransform, Transform)
public:
    MatrixTransform() noexcept : Transform(StaticTypeId()) {}
    Base::Transform2D GetMatrixValue() const noexcept;
    void SetMatrixValue(Base::Transform2D value) noexcept;
    inline static constexpr DependencyProperty<Base::Transform2D> MatrixProperty{"Matrix"};
    Base::Transform2D GetMatrix() const noexcept override {
        return GetMatrixValue();
    }
};
} // namespace Aero::Media

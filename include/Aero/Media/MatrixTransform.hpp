#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API MatrixTransform : public Transform {
    AERO_DECLARE_TYPE(MatrixTransform, Transform)
public:
    MatrixTransform() noexcept : Transform(StaticTypeId()) {}
    Base::Transform2D GetMatrixValue() const noexcept;
    void SetMatrixValue(Base::Transform2D value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Base::Transform2D, Matrix);
    Base::Transform2D GetMatrix() const noexcept override {
        return GetMatrixValue();
    }
};
} // namespace Aero::Media

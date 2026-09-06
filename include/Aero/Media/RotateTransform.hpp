#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API RotateTransform : public Transform {
    AERO_DECLARE_TYPE(RotateTransform, Transform)
public:
    RotateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetAngle() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetAngle(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Angle);
    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

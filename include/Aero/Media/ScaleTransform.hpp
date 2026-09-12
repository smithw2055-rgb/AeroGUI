#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API ScaleTransform : public Transform {
    AERO_DECLARE_TYPE(ScaleTransform, Transform)
public:
    ScaleTransform() noexcept : Transform(StaticTypeId()) {}
    double GetScaleX() const noexcept;
    double GetScaleY() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetScaleX(double value) noexcept;
    void SetScaleY(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, ScaleX);
    AERO_DEPENDENCY_PROPERTY(double, ScaleY);
    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

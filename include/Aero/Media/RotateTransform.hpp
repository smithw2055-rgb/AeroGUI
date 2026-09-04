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

    inline static constexpr DependencyProperty<double> AngleProperty{"Angle"};
    inline static constexpr DependencyProperty<double> CenterXProperty{"CenterX"};
    inline static constexpr DependencyProperty<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

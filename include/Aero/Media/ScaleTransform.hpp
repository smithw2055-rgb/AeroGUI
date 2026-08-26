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

    inline static constexpr DependencyProperty<double> ScaleXProperty{"ScaleX"};
    inline static constexpr DependencyProperty<double> ScaleYProperty{"ScaleY"};
    inline static constexpr DependencyProperty<double> CenterXProperty{"CenterX"};
    inline static constexpr DependencyProperty<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

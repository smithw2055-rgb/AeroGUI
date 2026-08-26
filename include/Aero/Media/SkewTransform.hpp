#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API SkewTransform : public Transform {
    AERO_DECLARE_TYPE(SkewTransform, Transform)
public:
    SkewTransform() noexcept : Transform(StaticTypeId()) {}
    double GetAngleX() const noexcept;
    double GetAngleY() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetAngleX(double value) noexcept;
    void SetAngleY(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    inline static constexpr DependencyProperty<double> AngleXProperty{"AngleX"};
    inline static constexpr DependencyProperty<double> AngleYProperty{"AngleY"};
    inline static constexpr DependencyProperty<double> CenterXProperty{"CenterX"};
    inline static constexpr DependencyProperty<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

#pragma once

#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API TranslateTransform : public Transform {
    AERO_DECLARE_TYPE(TranslateTransform, Transform)
public:
    TranslateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetX() const noexcept;
    double GetY() const noexcept;
    void SetX(double value) noexcept;
    void SetY(double value) noexcept;

    inline static constexpr DependencyProperty<double> XProperty{"X"};
    inline static constexpr DependencyProperty<double> YProperty{"Y"};

    Base::Transform2D GetMatrix() const noexcept override;
};
} // namespace Aero::Media

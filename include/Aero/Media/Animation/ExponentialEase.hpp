#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ExponentialEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ExponentialEase, EasingFunctionBase)
public:
    ExponentialEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Exponential) {}
    double GetExponent() const noexcept {
        return GetValueOr(ExponentProperty, 2.0);
    }
    void SetExponent(double value) noexcept;
    inline static constexpr DependencyProperty<double> ExponentProperty{"Exponent"};
};

} // namespace Aero::Media::Animation

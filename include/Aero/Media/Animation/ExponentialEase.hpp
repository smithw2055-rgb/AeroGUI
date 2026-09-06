#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ExponentialEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(ExponentialEase, EasingFunctionBase)
public:
    ExponentialEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Exponential) {}
    double GetExponent() const noexcept {
        return GetValue(ExponentProperty);
    }
    void SetExponent(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(double, Exponent);
};

} // namespace Aero::Media::Animation

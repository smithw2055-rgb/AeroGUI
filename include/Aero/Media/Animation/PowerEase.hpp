#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PowerEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(PowerEase, EasingFunctionBase)
public:
    PowerEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Power) {}
    double GetPower() const noexcept {
        return GetValue(PowerProperty);
    }
    void SetPower(double value) noexcept;
    inline static constexpr DependencyProperty<double> PowerProperty{"Power"};
};

} // namespace Aero::Media::Animation

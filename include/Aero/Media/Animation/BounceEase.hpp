#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API BounceEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BounceEase, EasingFunctionBase)
public:
    BounceEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Bounce) {}
    double GetBounces() const noexcept {
        return GetValueOr(BouncesProperty, 3.0);
    }
    double GetBounciness() const noexcept {
        return GetValueOr(BouncinessProperty, 3.0);
    }
    void SetBounces(double value) noexcept;
    void SetBounciness(double value) noexcept;
    inline static constexpr DependencyProperty<double> BouncesProperty{"Bounces"};
    inline static constexpr DependencyProperty<double> BouncinessProperty{"Bounciness"};
};

} // namespace Aero::Media::Animation

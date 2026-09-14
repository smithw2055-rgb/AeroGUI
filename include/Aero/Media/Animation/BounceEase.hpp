#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API BounceEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(BounceEase, EasingFunctionBase)
public:
    BounceEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Bounce) {}
    double GetBounces() const noexcept {
        return GetValue(BouncesProperty);
    }
    double GetBounciness() const noexcept {
        return GetValue(BouncinessProperty);
    }
    void SetBounces(double value) noexcept;
    void SetBounciness(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(double, Bounces);
    AERO_DEPENDENCY_PROPERTY(double, Bounciness);
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/DoubleAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DoubleAnimation : public DoubleAnimationBase {
    AERO_DECLARE_TYPE(DoubleAnimation, DoubleAnimationBase)
public:
    DoubleAnimation() noexcept : DoubleAnimationBase(StaticTypeId()) {}
    double GetAccelerationRatio() const noexcept {
        return accelerationRatio_;
    }
    double GetDecelerationRatio() const noexcept {
        return decelerationRatio_;
    }
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetAccelerationRatio(double value) noexcept;
    void SetDecelerationRatio(double value) noexcept;
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

protected:
    double GetCurrentValueCore(
        double defaultOriginValue,
        double defaultDestinationValue,
        double progress) const noexcept override {
        const double from = ResolveFrom(defaultOriginValue);
        const double to = ResolveTo(defaultDestinationValue);
        return from + (to - from) * progress;
    }

private:
    double accelerationRatio_ = 0.0;
    double decelerationRatio_ = 0.0;
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

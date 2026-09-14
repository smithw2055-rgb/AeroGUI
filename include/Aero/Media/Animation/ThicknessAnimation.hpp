#pragma once

#include <Aero/Media/Animation/ThicknessAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ThicknessAnimation : public ThicknessAnimationBase {
    AERO_DECLARE_TYPE(ThicknessAnimation, ThicknessAnimationBase)
public:
    ThicknessAnimation() noexcept : ThicknessAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

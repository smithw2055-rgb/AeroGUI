#pragma once

#include <Aero/Media/Animation/ColorAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ColorAnimation : public ColorAnimationBase {
    AERO_DECLARE_TYPE(ColorAnimation, ColorAnimationBase)
public:
    ColorAnimation() noexcept : ColorAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

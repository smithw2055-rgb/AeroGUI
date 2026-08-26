#pragma once

#include <Aero/Media/Animation/RectAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API RectAnimation : public RectAnimationBase {
    AERO_DECLARE_TYPE(RectAnimation, RectAnimationBase)
public:
    RectAnimation() noexcept : RectAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

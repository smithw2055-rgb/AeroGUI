#pragma once

#include <Aero/Media/Animation/SizeAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SizeAnimation : public SizeAnimationBase {
    AERO_DECLARE_TYPE(SizeAnimation, SizeAnimationBase)
public:
    SizeAnimation() noexcept : SizeAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

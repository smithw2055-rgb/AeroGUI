#pragma once

#include <Aero/Media/Animation/PointAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PointAnimation : public PointAnimationBase {
    AERO_DECLARE_TYPE(PointAnimation, PointAnimationBase)
public:
    PointAnimation() noexcept : PointAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

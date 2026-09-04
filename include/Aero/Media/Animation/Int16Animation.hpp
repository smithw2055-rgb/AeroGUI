#pragma once

#include <Aero/Media/Animation/Int16AnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int16Animation : public Int16AnimationBase {
    AERO_DECLARE_TYPE(Int16Animation, Int16AnimationBase)
public:
    Int16Animation() noexcept : Int16AnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

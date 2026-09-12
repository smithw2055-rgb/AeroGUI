#pragma once

#include <Aero/Media/Animation/Int64AnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int64Animation : public Int64AnimationBase {
    AERO_DECLARE_TYPE(Int64Animation, Int64AnimationBase)
public:
    Int64Animation() noexcept : Int64AnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

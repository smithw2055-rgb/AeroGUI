#pragma once

#include <Aero/Media/Animation/Int32AnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int32Animation : public Int32AnimationBase {
    AERO_DECLARE_TYPE(Int32Animation, Int32AnimationBase)
public:
    Int32Animation() noexcept : Int32AnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

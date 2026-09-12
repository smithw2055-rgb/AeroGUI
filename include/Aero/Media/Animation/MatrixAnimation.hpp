#pragma once

#include <Aero/Media/Animation/MatrixAnimationBase.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API MatrixAnimation : public MatrixAnimationBase {
    AERO_DECLARE_TYPE(MatrixAnimation, MatrixAnimationBase)
public:
    MatrixAnimation() noexcept : MatrixAnimationBase(StaticTypeId()) {}
    Ref<EasingFunctionBase> GetEasingFunction() const noexcept {
        return easing_;
    }
    void SetEasingFunction(Ref<EasingFunctionBase> value) noexcept;

private:
    Ref<EasingFunctionBase> easing_;
};

} // namespace Aero::Media::Animation

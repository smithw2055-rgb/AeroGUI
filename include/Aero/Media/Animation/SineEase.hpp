#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SineEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(SineEase, EasingFunctionBase)
public:
    SineEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Sine) {}
};

} // namespace Aero::Media::Animation

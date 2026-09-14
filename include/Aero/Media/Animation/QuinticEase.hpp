#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API QuinticEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(QuinticEase, EasingFunctionBase)
public:
    QuinticEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Quintic) {}
};

} // namespace Aero::Media::Animation

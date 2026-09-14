#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API QuarticEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(QuarticEase, EasingFunctionBase)
public:
    QuarticEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Quartic) {}
};

} // namespace Aero::Media::Animation

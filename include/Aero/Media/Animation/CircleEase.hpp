#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API CircleEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(CircleEase, EasingFunctionBase)
public:
    CircleEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Circle) {}
};

} // namespace Aero::Media::Animation

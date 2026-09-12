#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API CubicEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(CubicEase, EasingFunctionBase)
public:
    CubicEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Cubic) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/EasingFunctionBase.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API QuadraticEase : public EasingFunctionBase {
    AERO_DECLARE_TYPE(QuadraticEase, EasingFunctionBase)
public:
    QuadraticEase() noexcept
        : EasingFunctionBase(StaticTypeId(), Kind::Quadratic) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/ColorKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(DiscreteColorKeyFrame, ColorKeyFrame)
public:
    DiscreteColorKeyFrame() noexcept
        : ColorKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

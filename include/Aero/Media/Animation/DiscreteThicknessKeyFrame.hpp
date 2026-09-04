#pragma once

#include <Aero/Media/Animation/ThicknessKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteThicknessKeyFrame : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(DiscreteThicknessKeyFrame, ThicknessKeyFrame)
public:
    DiscreteThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

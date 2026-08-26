#pragma once

#include <Aero/Media/Animation/PointKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscretePointKeyFrame : public PointKeyFrame {
    AERO_DECLARE_TYPE(DiscretePointKeyFrame, PointKeyFrame)
public:
    DiscretePointKeyFrame() noexcept
        : PointKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

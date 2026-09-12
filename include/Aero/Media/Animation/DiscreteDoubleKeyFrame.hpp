#pragma once

#include <Aero/Media/Animation/DoubleKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(DiscreteDoubleKeyFrame, DoubleKeyFrame)
public:
    DiscreteDoubleKeyFrame() noexcept
        : DoubleKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/Int64KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteInt64KeyFrame : public Int64KeyFrame {
    AERO_DECLARE_TYPE(DiscreteInt64KeyFrame, Int64KeyFrame)
public:
    DiscreteInt64KeyFrame() noexcept
        : Int64KeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

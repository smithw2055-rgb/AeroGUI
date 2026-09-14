#pragma once

#include <Aero/Media/Animation/Int16KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteInt16KeyFrame : public Int16KeyFrame {
    AERO_DECLARE_TYPE(DiscreteInt16KeyFrame, Int16KeyFrame)
public:
    DiscreteInt16KeyFrame() noexcept
        : Int16KeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

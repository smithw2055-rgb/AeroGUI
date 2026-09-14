#pragma once

#include <Aero/Media/Animation/Int32KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteInt32KeyFrame : public Int32KeyFrame {
    AERO_DECLARE_TYPE(DiscreteInt32KeyFrame, Int32KeyFrame)
public:
    DiscreteInt32KeyFrame() noexcept
        : Int32KeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

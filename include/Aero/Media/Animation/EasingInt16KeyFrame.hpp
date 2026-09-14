#pragma once

#include <Aero/Media/Animation/Int16KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingInt16KeyFrame : public Int16KeyFrame {
    AERO_DECLARE_TYPE(EasingInt16KeyFrame, Int16KeyFrame)
public:
    EasingInt16KeyFrame() noexcept
        : Int16KeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/Int64KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingInt64KeyFrame : public Int64KeyFrame {
    AERO_DECLARE_TYPE(EasingInt64KeyFrame, Int64KeyFrame)
public:
    EasingInt64KeyFrame() noexcept
        : Int64KeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

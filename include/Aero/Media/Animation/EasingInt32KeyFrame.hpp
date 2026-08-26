#pragma once

#include <Aero/Media/Animation/Int32KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingInt32KeyFrame : public Int32KeyFrame {
    AERO_DECLARE_TYPE(EasingInt32KeyFrame, Int32KeyFrame)
public:
    EasingInt32KeyFrame() noexcept
        : Int32KeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

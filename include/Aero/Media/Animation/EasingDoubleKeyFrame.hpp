#pragma once

#include <Aero/Media/Animation/DoubleKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(EasingDoubleKeyFrame, DoubleKeyFrame)
public:
    EasingDoubleKeyFrame() noexcept
        : DoubleKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

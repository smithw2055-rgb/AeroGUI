#pragma once

#include <Aero/Media/Animation/PointKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingPointKeyFrame : public PointKeyFrame {
    AERO_DECLARE_TYPE(EasingPointKeyFrame, PointKeyFrame)
public:
    EasingPointKeyFrame() noexcept
        : PointKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

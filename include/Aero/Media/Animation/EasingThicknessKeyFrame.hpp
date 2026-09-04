#pragma once

#include <Aero/Media/Animation/ThicknessKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingThicknessKeyFrame : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(EasingThicknessKeyFrame, ThicknessKeyFrame)
public:
    EasingThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

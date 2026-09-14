#pragma once

#include <Aero/Media/Animation/ColorKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(EasingColorKeyFrame, ColorKeyFrame)
public:
    EasingColorKeyFrame() noexcept
        : ColorKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

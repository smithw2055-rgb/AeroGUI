#pragma once

#include <Aero/Media/Animation/ColorKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(LinearColorKeyFrame, ColorKeyFrame)
public:
    LinearColorKeyFrame() noexcept
        : ColorKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/ThicknessKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearThicknessKeyFrame : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(LinearThicknessKeyFrame, ThicknessKeyFrame)
public:
    LinearThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

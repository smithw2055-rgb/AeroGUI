#pragma once

#include <Aero/Media/Animation/ThicknessKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineThicknessKeyFrame : public ThicknessKeyFrame {
    AERO_DECLARE_TYPE(SplineThicknessKeyFrame, ThicknessKeyFrame)
public:
    SplineThicknessKeyFrame() noexcept
        : ThicknessKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

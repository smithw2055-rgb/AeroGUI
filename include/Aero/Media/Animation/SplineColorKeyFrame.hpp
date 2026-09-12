#pragma once

#include <Aero/Media/Animation/ColorKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineColorKeyFrame : public ColorKeyFrame {
    AERO_DECLARE_TYPE(SplineColorKeyFrame, ColorKeyFrame)
public:
    SplineColorKeyFrame() noexcept
        : ColorKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/DoubleKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(SplineDoubleKeyFrame, DoubleKeyFrame)
public:
    SplineDoubleKeyFrame() noexcept
        : DoubleKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

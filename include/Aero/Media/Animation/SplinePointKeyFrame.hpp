#pragma once

#include <Aero/Media/Animation/PointKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplinePointKeyFrame : public PointKeyFrame {
    AERO_DECLARE_TYPE(SplinePointKeyFrame, PointKeyFrame)
public:
    SplinePointKeyFrame() noexcept
        : PointKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

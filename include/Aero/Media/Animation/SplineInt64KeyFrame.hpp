#pragma once

#include <Aero/Media/Animation/Int64KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineInt64KeyFrame : public Int64KeyFrame {
    AERO_DECLARE_TYPE(SplineInt64KeyFrame, Int64KeyFrame)
public:
    SplineInt64KeyFrame() noexcept
        : Int64KeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

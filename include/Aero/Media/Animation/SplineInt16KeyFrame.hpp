#pragma once

#include <Aero/Media/Animation/Int16KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineInt16KeyFrame : public Int16KeyFrame {
    AERO_DECLARE_TYPE(SplineInt16KeyFrame, Int16KeyFrame)
public:
    SplineInt16KeyFrame() noexcept
        : Int16KeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

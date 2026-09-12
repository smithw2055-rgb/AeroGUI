#pragma once

#include <Aero/Media/Animation/Int32KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineInt32KeyFrame : public Int32KeyFrame {
    AERO_DECLARE_TYPE(SplineInt32KeyFrame, Int32KeyFrame)
public:
    SplineInt32KeyFrame() noexcept
        : Int32KeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

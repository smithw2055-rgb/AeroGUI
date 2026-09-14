#pragma once

#include <Aero/Media/Animation/Int64KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearInt64KeyFrame : public Int64KeyFrame {
    AERO_DECLARE_TYPE(LinearInt64KeyFrame, Int64KeyFrame)
public:
    LinearInt64KeyFrame() noexcept
        : Int64KeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

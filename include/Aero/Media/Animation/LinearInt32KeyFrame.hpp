#pragma once

#include <Aero/Media/Animation/Int32KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearInt32KeyFrame : public Int32KeyFrame {
    AERO_DECLARE_TYPE(LinearInt32KeyFrame, Int32KeyFrame)
public:
    LinearInt32KeyFrame() noexcept
        : Int32KeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

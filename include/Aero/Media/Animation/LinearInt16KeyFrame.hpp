#pragma once

#include <Aero/Media/Animation/Int16KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearInt16KeyFrame : public Int16KeyFrame {
    AERO_DECLARE_TYPE(LinearInt16KeyFrame, Int16KeyFrame)
public:
    LinearInt16KeyFrame() noexcept
        : Int16KeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

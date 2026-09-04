#pragma once

#include <Aero/Media/Animation/PointKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearPointKeyFrame : public PointKeyFrame {
    AERO_DECLARE_TYPE(LinearPointKeyFrame, PointKeyFrame)
public:
    LinearPointKeyFrame() noexcept
        : PointKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

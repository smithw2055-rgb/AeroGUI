#pragma once

#include <Aero/Media/Animation/BooleanKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteBooleanKeyFrame : public BooleanKeyFrame {
    AERO_DECLARE_TYPE(DiscreteBooleanKeyFrame, BooleanKeyFrame)
public:
    DiscreteBooleanKeyFrame() noexcept
        : BooleanKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/SizeKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteSizeKeyFrame : public SizeKeyFrame {
    AERO_DECLARE_TYPE(DiscreteSizeKeyFrame, SizeKeyFrame)
public:
    DiscreteSizeKeyFrame() noexcept
        : SizeKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

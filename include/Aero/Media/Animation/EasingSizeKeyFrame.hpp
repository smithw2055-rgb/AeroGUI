#pragma once

#include <Aero/Media/Animation/SizeKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingSizeKeyFrame : public SizeKeyFrame {
    AERO_DECLARE_TYPE(EasingSizeKeyFrame, SizeKeyFrame)
public:
    EasingSizeKeyFrame() noexcept
        : SizeKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

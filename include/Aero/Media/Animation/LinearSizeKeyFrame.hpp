#pragma once

#include <Aero/Media/Animation/SizeKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearSizeKeyFrame : public SizeKeyFrame {
    AERO_DECLARE_TYPE(LinearSizeKeyFrame, SizeKeyFrame)
public:
    LinearSizeKeyFrame() noexcept
        : SizeKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

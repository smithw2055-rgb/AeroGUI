#pragma once

#include <Aero/Media/Animation/SizeKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineSizeKeyFrame : public SizeKeyFrame {
    AERO_DECLARE_TYPE(SplineSizeKeyFrame, SizeKeyFrame)
public:
    SplineSizeKeyFrame() noexcept
        : SizeKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

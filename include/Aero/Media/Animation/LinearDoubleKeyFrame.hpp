#pragma once

#include <Aero/Media/Animation/DoubleKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearDoubleKeyFrame : public DoubleKeyFrame {
    AERO_DECLARE_TYPE(LinearDoubleKeyFrame, DoubleKeyFrame)
public:
    LinearDoubleKeyFrame() noexcept
        : DoubleKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

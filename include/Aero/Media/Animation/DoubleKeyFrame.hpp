#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DoubleKeyFrame : public KeyFrame<double> {
    AERO_DECLARE_TYPE(DoubleKeyFrame, KeyFrameBase)
protected:
    explicit DoubleKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<double>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API BooleanKeyFrame : public KeyFrame<bool> {
    AERO_DECLARE_TYPE(BooleanKeyFrame, KeyFrameBase)
protected:
    explicit BooleanKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<bool>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

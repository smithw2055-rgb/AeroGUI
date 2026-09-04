#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SizeKeyFrame : public KeyFrame<Base::Size> {
    AERO_DECLARE_TYPE(SizeKeyFrame, KeyFrameBase)
protected:
    explicit SizeKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::Size>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

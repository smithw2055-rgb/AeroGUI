#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ObjectKeyFrame : public KeyFrame<Meta::PropertyValue> {
    AERO_DECLARE_TYPE(ObjectKeyFrame, KeyFrameBase)
protected:
    explicit ObjectKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Meta::PropertyValue>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

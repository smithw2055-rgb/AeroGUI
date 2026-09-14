#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API StringKeyFrame : public KeyFrame<Base::String> {
    AERO_DECLARE_TYPE(StringKeyFrame, KeyFrameBase)
protected:
    explicit StringKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::String>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

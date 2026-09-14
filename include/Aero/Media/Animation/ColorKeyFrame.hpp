#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ColorKeyFrame : public KeyFrame<Base::Color> {
    AERO_DECLARE_TYPE(ColorKeyFrame, KeyFrameBase)
protected:
    explicit ColorKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::Color>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

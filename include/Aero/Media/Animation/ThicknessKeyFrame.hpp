#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ThicknessKeyFrame : public KeyFrame<Base::Thickness> {
    AERO_DECLARE_TYPE(ThicknessKeyFrame, KeyFrameBase)
protected:
    explicit ThicknessKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::Thickness>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

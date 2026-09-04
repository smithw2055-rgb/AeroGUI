#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PointKeyFrame : public KeyFrame<Base::Point> {
    AERO_DECLARE_TYPE(PointKeyFrame, KeyFrameBase)
protected:
    explicit PointKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::Point>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

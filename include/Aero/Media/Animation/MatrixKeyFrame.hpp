#pragma once

#include <Aero/Media/Animation/KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API MatrixKeyFrame : public KeyFrame<Base::Transform2D> {
    AERO_DECLARE_TYPE(MatrixKeyFrame, KeyFrameBase)
protected:
    explicit MatrixKeyFrame(
        Meta::TypeId runtimeType,
        Interpolation interpolation) noexcept
        : KeyFrame<Base::Transform2D>(runtimeType, interpolation) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/MatrixKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API EasingMatrixKeyFrame : public MatrixKeyFrame {
    AERO_DECLARE_TYPE(EasingMatrixKeyFrame, MatrixKeyFrame)
public:
    EasingMatrixKeyFrame() noexcept
        : MatrixKeyFrame(StaticTypeId(), Interpolation::Easing) {}
};

} // namespace Aero::Media::Animation

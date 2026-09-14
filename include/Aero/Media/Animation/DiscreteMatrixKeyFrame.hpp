#pragma once

#include <Aero/Media/Animation/MatrixKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DiscreteMatrixKeyFrame : public MatrixKeyFrame {
    AERO_DECLARE_TYPE(DiscreteMatrixKeyFrame, MatrixKeyFrame)
public:
    DiscreteMatrixKeyFrame() noexcept
        : MatrixKeyFrame(StaticTypeId(), Interpolation::Discrete) {}
};

} // namespace Aero::Media::Animation

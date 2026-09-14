#pragma once

#include <Aero/Media/Animation/MatrixKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API LinearMatrixKeyFrame : public MatrixKeyFrame {
    AERO_DECLARE_TYPE(LinearMatrixKeyFrame, MatrixKeyFrame)
public:
    LinearMatrixKeyFrame() noexcept
        : MatrixKeyFrame(StaticTypeId(), Interpolation::Linear) {}
};

} // namespace Aero::Media::Animation

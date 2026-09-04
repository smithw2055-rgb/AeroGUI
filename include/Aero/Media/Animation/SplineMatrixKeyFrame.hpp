#pragma once

#include <Aero/Media/Animation/MatrixKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SplineMatrixKeyFrame : public MatrixKeyFrame {
    AERO_DECLARE_TYPE(SplineMatrixKeyFrame, MatrixKeyFrame)
public:
    SplineMatrixKeyFrame() noexcept
        : MatrixKeyFrame(StaticTypeId(), Interpolation::Spline) {}
};

} // namespace Aero::Media::Animation

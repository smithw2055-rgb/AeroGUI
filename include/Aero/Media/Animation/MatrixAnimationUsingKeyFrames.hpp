#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/MatrixKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API MatrixAnimationUsingKeyFrames
    : public AnimationUsingKeyFrames<MatrixKeyFrame> {
    AERO_DECLARE_TYPE(MatrixAnimationUsingKeyFrames, AnimationTimeline)
public:
    MatrixAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<MatrixKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

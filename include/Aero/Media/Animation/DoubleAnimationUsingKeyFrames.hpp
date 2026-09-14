#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/DoubleKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API DoubleAnimationUsingKeyFrames : public AnimationUsingKeyFrames<DoubleKeyFrame> {
    AERO_DECLARE_TYPE(DoubleAnimationUsingKeyFrames, AnimationTimeline)
public:
    DoubleAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<DoubleKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/BooleanKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API BooleanAnimationUsingKeyFrames : public AnimationUsingKeyFrames<BooleanKeyFrame> {
    AERO_DECLARE_TYPE(BooleanAnimationUsingKeyFrames, AnimationTimeline)
public:
    BooleanAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<BooleanKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

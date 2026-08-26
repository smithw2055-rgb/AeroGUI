#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/ObjectKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ObjectAnimationUsingKeyFrames : public AnimationUsingKeyFrames<ObjectKeyFrame> {
    AERO_DECLARE_TYPE(ObjectAnimationUsingKeyFrames, AnimationTimeline)
public:
    ObjectAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<ObjectKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

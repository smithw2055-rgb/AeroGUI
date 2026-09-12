#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/ThicknessKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ThicknessAnimationUsingKeyFrames : public AnimationUsingKeyFrames<ThicknessKeyFrame> {
    AERO_DECLARE_TYPE(ThicknessAnimationUsingKeyFrames, AnimationTimeline)
public:
    ThicknessAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<ThicknessKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

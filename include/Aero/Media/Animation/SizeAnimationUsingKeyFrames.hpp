#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/SizeKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API SizeAnimationUsingKeyFrames : public AnimationUsingKeyFrames<SizeKeyFrame> {
    AERO_DECLARE_TYPE(SizeAnimationUsingKeyFrames, AnimationTimeline)
public:
    SizeAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<SizeKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

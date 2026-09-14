#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/ColorKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ColorAnimationUsingKeyFrames : public AnimationUsingKeyFrames<ColorKeyFrame> {
    AERO_DECLARE_TYPE(ColorAnimationUsingKeyFrames, AnimationTimeline)
public:
    ColorAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<ColorKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/StringKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API StringAnimationUsingKeyFrames : public AnimationUsingKeyFrames<StringKeyFrame> {
    AERO_DECLARE_TYPE(StringAnimationUsingKeyFrames, AnimationTimeline)
public:
    StringAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<StringKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

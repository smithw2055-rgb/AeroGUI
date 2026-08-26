#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/Int64KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int64AnimationUsingKeyFrames : public AnimationUsingKeyFrames<Int64KeyFrame> {
    AERO_DECLARE_TYPE(Int64AnimationUsingKeyFrames, AnimationTimeline)
public:
    Int64AnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<Int64KeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

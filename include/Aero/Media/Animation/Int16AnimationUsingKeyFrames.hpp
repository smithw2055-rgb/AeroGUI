#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/Int16KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int16AnimationUsingKeyFrames : public AnimationUsingKeyFrames<Int16KeyFrame> {
    AERO_DECLARE_TYPE(Int16AnimationUsingKeyFrames, AnimationTimeline)
public:
    Int16AnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<Int16KeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

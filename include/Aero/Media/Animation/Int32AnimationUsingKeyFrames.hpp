#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/Int32KeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API Int32AnimationUsingKeyFrames : public AnimationUsingKeyFrames<Int32KeyFrame> {
    AERO_DECLARE_TYPE(Int32AnimationUsingKeyFrames, AnimationTimeline)
public:
    Int32AnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<Int32KeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

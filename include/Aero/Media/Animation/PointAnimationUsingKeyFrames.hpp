#pragma once

#include <Aero/Media/Animation/AnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/PointKeyFrame.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PointAnimationUsingKeyFrames : public AnimationUsingKeyFrames<PointKeyFrame> {
    AERO_DECLARE_TYPE(PointAnimationUsingKeyFrames, AnimationTimeline)
public:
    PointAnimationUsingKeyFrames() noexcept
        : AnimationUsingKeyFrames<PointKeyFrame>(StaticTypeId()) {}
};

} // namespace Aero::Media::Animation

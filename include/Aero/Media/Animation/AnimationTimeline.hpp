#pragma once

#include <Aero/Media/Animation/Timeline.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API AnimationTimeline : public Timeline {
    AERO_DECLARE_TYPE(AnimationTimeline, Timeline)
protected:
    explicit AnimationTimeline(Meta::TypeId runtimeType) noexcept
        : Timeline(runtimeType) {}
};

} // namespace Aero::Media::Animation

#pragma once

#include <Aero/Media/Animation/TimelineGroup.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ParallelTimeline : public TimelineGroup {
    AERO_DECLARE_TYPE(ParallelTimeline, TimelineGroup)
public:
    ParallelTimeline() noexcept : ParallelTimeline(StaticTypeId()) {}

protected:
    explicit ParallelTimeline(Meta::TypeId runtimeType) noexcept
        : TimelineGroup(runtimeType) {}
};

} // namespace Aero::Media::Animation

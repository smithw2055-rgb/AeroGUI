#pragma once

// Shared helpers for StoryboardHost translation units.

#include "gui/ViewState.hpp"

#include <Aero/Media/Animation/EventTrigger.hpp>

namespace Aero {
namespace StoryboardDetail {

template<class TAnimation>
inline Aero::Media::Animation::TimelineRuntime::KeyframeSchedule
MakeKeyframeSchedule(
    const TAnimation& animation,
    Aero::Media::Animation::AnimationTime authoredDuration) noexcept {
    return Aero::Media::Animation::TimelineRuntime::MakeSchedule(
        animation.GetKeyFrames(), authoredDuration);
}

} // namespace StoryboardDetail

using StoryboardDetail::MakeKeyframeSchedule;

} // namespace Aero

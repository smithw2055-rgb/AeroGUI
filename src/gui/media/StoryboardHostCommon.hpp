#pragma once

// Shared helpers for StoryboardHost translation units.

#include "gui/ViewState.hpp"

#include <Aero/Media/Animation/EventTrigger.hpp>

namespace Aero {
namespace StoryboardSupport {

template<class TAnimation>
inline Aero::Media::Animation::KeyframeSchedule
MakeKeyframeSchedule(
    const TAnimation& animation,
    Aero::Media::Animation::AnimationTime authoredDuration) noexcept {
    return Aero::Media::Animation::MakeSchedule(
        animation.GetKeyFrames(), authoredDuration);
}

} // namespace StoryboardSupport

using StoryboardSupport::MakeKeyframeSchedule;

} // namespace Aero

#include "gui/core/EnumRegistration.hpp"

#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Media/Animation/EasingFunctions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>

namespace Aero {

Base::Result<void> PopulateAnimationEnums(
    Meta::Registration& context) noexcept {
    using namespace Media::Animation;
    // Prefer public Animation enums over Model aliases (AnimationEngine.hpp).
    using Media::Animation::FillBehavior;
    using Media::Animation::EasingMode;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        FillBehavior,
        "FillBehavior",
        description
            .Value("HoldEnd", FillBehavior::HoldEnd)
            .Value("Stop", FillBehavior::Stop););
    AERO_REGISTER_ENUM(
        EasingMode,
        "EasingMode",
        description
            .Value("EaseOut", EasingMode::EaseOut)
            .Value("EaseIn", EasingMode::EaseIn)
            .Value("EaseInOut", EasingMode::EaseInOut););
    AERO_REGISTER_ENUM(
        ControlStoryboardAction::Option,
        "ControlStoryboardOption",
        description
            .Value("Play", ControlStoryboardAction::Option::Play)
            .Value("Stop", ControlStoryboardAction::Option::Stop)
            .Value("TogglePlayPause", ControlStoryboardAction::Option::TogglePlayPause)
            .Value("Pause", ControlStoryboardAction::Option::Pause)
            .Value("Resume", ControlStoryboardAction::Option::Resume)
            .Value("SkipToFill", ControlStoryboardAction::Option::SkipToFill););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero

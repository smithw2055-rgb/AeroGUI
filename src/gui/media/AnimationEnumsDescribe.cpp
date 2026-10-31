#include "gui/core/EnumRegistration.hpp"

#include <Aero/Media/Animation/Timeline.hpp>
#include <Aero/Media/Animation/EasingFunctions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>

namespace Aero {

void PopulateAnimationEnums(
    Meta::Registration& context) noexcept {
    using namespace Media::Animation;
    using Media::Animation::FillBehavior;
    using Media::Animation::EasingMode;

    AERO_AUTO_REGISTER_ENUM(FillBehavior);
    AERO_AUTO_REGISTER_ENUM(EasingMode);
    AERO_AUTO_REGISTER_NAMED_ENUM(ControlStoryboardAction::Option, "ControlStoryboardOption");
}

} // namespace Aero

#pragma once

#include <Aero/Media/Animation/ControllableStoryboardAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API StopStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(StopStoryboard, ControllableStoryboardAction)
public:
    StopStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};
} // namespace Aero::Media::Animation

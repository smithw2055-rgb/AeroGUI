#pragma once

#include <Aero/Media/Animation/ControllableStoryboardAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API PauseStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(PauseStoryboard, ControllableStoryboardAction)
public:
    PauseStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};
} // namespace Aero::Media::Animation

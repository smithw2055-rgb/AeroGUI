#pragma once

#include <Aero/Media/Animation/ControllableStoryboardAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API RemoveStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(RemoveStoryboard, ControllableStoryboardAction)
public:
    RemoveStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};
} // namespace Aero::Media::Animation

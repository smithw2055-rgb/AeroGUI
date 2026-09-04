#pragma once

#include <Aero/Media/Animation/ControllableStoryboardAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ResumeStoryboard : public ControllableStoryboardAction {
    AERO_DECLARE_TYPE(ResumeStoryboard, ControllableStoryboardAction)
public:
    ResumeStoryboard() noexcept : ControllableStoryboardAction(StaticTypeId()) {}
};
} // namespace Aero::Media::Animation

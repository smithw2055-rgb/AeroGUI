#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>

using Aero::Interactivity::TriggerAction;

namespace Aero::Media::Animation {

class AERO_GUI_API PauseMediaAction : public TriggerAction {
    AERO_DECLARE_TYPE(PauseMediaAction, TriggerAction)
public:
    PauseMediaAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept {
        (void)targetName_.Assign(value);
    }

private:
    String targetName_;
};
} // namespace Aero::Media::Animation

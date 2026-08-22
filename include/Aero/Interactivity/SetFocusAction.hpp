#pragma once

#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Interactivity {

class AERO_GUI_API SetFocusAction : public TriggerAction {
    AERO_DECLARE_TYPE(SetFocusAction, TriggerAction)
public:
    SetFocusAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    void SetTargetName(StringView value) noexcept {
        (void)targetName_.Assign(value);
    }
    bool GetEngage() const noexcept { return engage_; }
    void SetEngage(bool value) noexcept { engage_ = value; }

private:
    String targetName_;
    bool engage_ = true;
};

} // namespace Aero::Interactivity

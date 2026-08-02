#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_API SetFocusAction : public TriggerAction {
    AERO_DECLARE_TYPE(SetFocusAction, TriggerAction)
public:
    SetFocusAction() noexcept : TriggerAction(StaticTypeId()) {}
    bool GetEngage() const noexcept { return engage_; }
    void SetEngage(bool value) noexcept { engage_ = value; }

private:
    bool engage_ = true;
};

} // namespace Aero::Media::Animation

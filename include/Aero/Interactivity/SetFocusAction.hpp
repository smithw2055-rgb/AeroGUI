#pragma once

#include <Aero/Data/Binding.hpp>
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
    Ref<Aero::Data::Binding> GetTargetObject() const noexcept {
        return targetObject_;
    }
    void SetTargetObject(Ref<Aero::Data::Binding> value) noexcept {
        targetObject_ = std::move(value);
    }
    bool GetEngage() const noexcept { return engage_; }
    void SetEngage(bool value) noexcept { engage_ = value; }

private:
    String targetName_;
    Ref<Aero::Data::Binding> targetObject_;
    bool engage_ = true;
};

} // namespace Aero::Interactivity

#pragma once

#include <Aero/Data/Binding.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Interactivity {

class AERO_GUI_API RemoveElementAction : public TriggerAction {
    AERO_DECLARE_TYPE(RemoveElementAction, TriggerAction)
public:
    RemoveElementAction() noexcept : TriggerAction(StaticTypeId()) {}
    Ref<Aero::Data::Binding> GetTargetObject() const noexcept {
        return targetObject_;
    }
    void SetTargetObject(Ref<Aero::Data::Binding> value) noexcept {
        targetObject_ = std::move(value);
    }

private:
    Ref<Aero::Data::Binding> targetObject_;
};

} // namespace Aero::Interactivity

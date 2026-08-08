#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API RemoveElementAction : public TriggerAction {
    AERO_DECLARE_TYPE(RemoveElementAction, TriggerAction)
public:
    RemoveElementAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::Ref<Aero::Data::Binding> GetTargetObject() const noexcept {
        return targetObject_;
    }
    void SetTargetObject(Base::Ref<Aero::Data::Binding> value) noexcept {
        targetObject_ = std::move(value);
    }

private:
    Base::Ref<Aero::Data::Binding> targetObject_;
};

} // namespace Aero::Media::Animation

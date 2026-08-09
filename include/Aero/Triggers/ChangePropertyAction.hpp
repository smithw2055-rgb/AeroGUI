#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ChangePropertyAction : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetTargetName() const noexcept { return targetName_.View(); }
    StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    const Meta::PropertyValue& GetValue() const noexcept { return value_; }
    Ref<Aero::Data::Binding> GetValueBinding() const noexcept {
        return valueBinding_;
    }
    void SetTargetName(StringView value) noexcept;
    void SetPropertyName(StringView value) noexcept;
    void SetValue(const Meta::PropertyValue& value) noexcept;
    void SetValueBinding(
        Ref<Aero::Data::Binding> value) noexcept;

private:
    String targetName_;
    String propertyName_;
    Meta::PropertyValue value_;
    Ref<Aero::Data::Binding> valueBinding_;
};

} // namespace Aero::Media::Animation

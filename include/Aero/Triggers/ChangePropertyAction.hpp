#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_GUI_API ChangePropertyAction : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::StringView GetTargetName() const noexcept { return targetName_.View(); }
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    const Meta::PropertyValue& GetValue() const noexcept { return value_; }
    Base::Ref<Aero::Data::Binding> GetValueBinding() const noexcept {
        return valueBinding_;
    }
    void SetTargetName(Base::StringView value) noexcept;
    void SetPropertyName(Base::StringView value) noexcept;
    void SetValue(const Meta::PropertyValue& value) noexcept;
    void SetValueBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept;

private:
    Base::String targetName_;
    Base::String propertyName_;
    Meta::PropertyValue value_;
    Base::Ref<Aero::Data::Binding> valueBinding_;
};

} // namespace Aero::Media::Animation

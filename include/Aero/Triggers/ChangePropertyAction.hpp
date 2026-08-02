#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_API ChangePropertyAction : public TriggerAction {
    AERO_DECLARE_TYPE(ChangePropertyAction, TriggerAction)
public:
    ChangePropertyAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::StringView GetTargetName() const noexcept { return targetName_.View(); }
    Base::StringView GetPropertyName() const noexcept { return propertyName_.View(); }
    const Meta::PropertyValue& GetValue() const noexcept { return value_; }
    void SetTargetName(Base::StringView value) noexcept;
    void SetPropertyName(Base::StringView value) noexcept;
    void SetValue(const Meta::PropertyValue& value) noexcept;

private:
    Base::String targetName_;
    Base::String propertyName_;
    Meta::PropertyValue value_;
};

} // namespace Aero::Media::Animation

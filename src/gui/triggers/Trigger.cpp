#include <Aero/Style.hpp>
#include <Aero/Triggers/Trigger.hpp>

#include "gui/meta/ValueConversion.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

void Trigger::SetProperty(
    DependencyPropertyHandle value) noexcept {
    if (!value.IsValid()) return;
    property_ = value;
}

void Trigger::SetValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    value_ = value;
}

Base::Result<void> Trigger::AddSetter(
    const Setter& setter) noexcept {
    if (!setter.GetProperty().IsValid() ||
        setter.GetValue().IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Trigger setter is invalid");
    }
    Base::Result<void> property =
        setterProperties_.PushBack(setter.GetProperty());
    if (!property) return property.GetStatus();
    Base::Result<void> value =
        setterValues_.PushBack(setter.GetValue());
    if (!value) {
        setterProperties_.PopBack();
        return value.GetStatus();
    }
    return {};
}

void Trigger::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) return;
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    propertyName_ = std::move(candidate);
}

void Trigger::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

void Trigger::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
}

Base::Result<void> Trigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "Trigger authored setter is null");
    }
    return authoredSetters_.PushBack(
        std::move(setter));
}

void Trigger::ClearAuthoredSetters() noexcept {
    authoredSetters_.Clear();
}

} // namespace Aero

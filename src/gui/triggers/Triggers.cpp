#include <Aero/Style.hpp>
#include <Aero/Triggers/TriggerBase.hpp>
#include <Aero/Triggers/Trigger.hpp>
#include <Aero/Triggers/DataTrigger.hpp>
#include <Aero/Triggers/MultiTrigger.hpp>
#include <Aero/Triggers/MultiDataTrigger.hpp>
#include <Aero/Triggers/Conditions.hpp>
#include <Aero/Value.hpp>

#include "gui/data/BindingEngine.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"

namespace Aero {

// ===== TriggerBase =====

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject()) {
        return false;
    }
    if (value.Type() == Data::Binding::StaticTypeId()) {
        return true;
    }
    return value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::Binding::StaticTypeId();
}

void TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    enterActions_.PushBack(
        std::move(action));
}

void TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    exitActions_.PushBack(
        std::move(action));
}

// ===== Trigger =====

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

void Trigger::AddSetter(
    const Setter& setter) noexcept {
    if (!setter.GetProperty().IsValid() ||
        setter.GetValue().IsUnset()) { AERO_ASSERT(false); return; }
    setterProperties_.PushBack(setter.GetProperty());
    setterValues_.PushBack(setter.GetValue());
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

void Trigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

void Trigger::ClearAuthoredSetters() noexcept {
    authoredSetters_.Clear();
}

// ===== DataTrigger =====

void DataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

void DataTrigger::SetPropertyName(StringView value) noexcept {
    static_cast<void>(propertyName_.Assign(value));
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(value));
}

// ===== MultiTrigger =====

void MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(std::move(condition));
}

void MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(std::move(setter));
}

// ===== MultiDataTrigger =====

void MultiDataTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(
        std::move(condition));
}

void MultiDataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(
        std::move(setter));
}

// ===== Condition =====

void Condition::SetPropertyName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    propertyName_ = std::move(candidate);
}

void Condition::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

} // namespace Aero

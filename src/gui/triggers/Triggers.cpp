#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Events.hpp>
#include <Aero/Value.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>

#include "gui/core/Describe.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"

#include <utility>


namespace Aero {

// ===== TriggerBase =====

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    if (value.IsDynamicResource()) {
        return true;
    }
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject()) {
        return false;
    }
    if (value.Type() == Data::Binding::StaticTypeId()) {
        return true;
    }
    if (value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::Binding::StaticTypeId()) {
        return true;
    }
    return false;
}

bool IsDynamicResourceSetterValue(
    const PropertyValue& value,
    Base::StringView& key) noexcept {
    if (value.IsDynamicResource()) {
        key = value.AsDynamicResourceKey();
        return true;
    }
    return false;
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
    auto s = Base::MakeRef<Setter>();
    if (!s) return;
    s->SetProperty(setter.GetProperty());
    s->SetValue(setter.GetValue());
    AddAuthoredSetter(std::move(s));
}

namespace {

inline AuthoredTriggerData* EnsureAuthoredData(AuthoredTriggerData*& authored) noexcept {
    if (authored == nullptr) {
        authored = new (std::nothrow) AuthoredTriggerData();
    }
    return authored;
}

inline void ReleaseAuthoredData(AuthoredTriggerData*& authored) noexcept {
    delete authored;
    authored = nullptr;
}

inline StringView GetAuthoredPropertyName(const AuthoredTriggerData* authored) noexcept {
    return authored != nullptr ? authored->propertyName.View() : StringView{};
}

inline StringView GetAuthoredSourceName(const AuthoredTriggerData* authored) noexcept {
    return authored != nullptr ? authored->sourceName.View() : StringView{};
}

inline const PropertyValue& GetAuthoredPropertyValue(const AuthoredTriggerData* authored) noexcept {
    static const PropertyValue emptyValue{};
    return authored != nullptr ? authored->authoredValue : emptyValue;
}

inline void SetAuthoredPropertyName(AuthoredTriggerData*& authored, StringView value, bool trim = false) noexcept {
    if (value.Empty()) return;
    AuthoredTriggerData* data = EnsureAuthoredData(authored);
    if (data == nullptr) return;
    Base::String candidate;
    if (trim) {
        if (!candidate.Assign(::Aero::Base::ValueConversion::Trim(value))) return;
    } else {
        if (!candidate.Assign(value)) return;
    }
    data->propertyName = std::move(candidate);
}

inline void SetAuthoredSourceName(AuthoredTriggerData*& authored, StringView value) noexcept {
    AuthoredTriggerData* data = EnsureAuthoredData(authored);
    if (data == nullptr) return;
    Base::String candidate;
    if (!candidate.Assign(::Aero::Base::ValueConversion::Trim(value))) return;
    data->sourceName = std::move(candidate);
}

inline void SetAuthoredPropertyValue(AuthoredTriggerData*& authored, const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    AuthoredTriggerData* data = EnsureAuthoredData(authored);
    if (data == nullptr) return;
    data->authoredValue = value;
}

} // namespace

Trigger::~Trigger() {
    ReleaseAuthored();
}

Trigger::AuthoredData* Trigger::EnsureAuthored() noexcept {
    return EnsureAuthoredData(authored_);
}

void Trigger::ReleaseAuthored() noexcept {
    ReleaseAuthoredData(authored_);
}

StringView Trigger::GetPropertyName() const noexcept {
    return GetAuthoredPropertyName(authored_);
}

StringView Trigger::GetSourceName() const noexcept {
    return GetAuthoredSourceName(authored_);
}

const PropertyValue& Trigger::GetAuthoredValue() const noexcept {
    return GetAuthoredPropertyValue(authored_);
}

bool Trigger::GetIsAuthored() const noexcept {
    return authored_ != nullptr && !authored_->propertyName.Empty() && !authored_->authoredValue.IsUnset();
}

void Trigger::SetPropertyName(
    Base::StringView value) noexcept {
    SetAuthoredPropertyName(authored_, value);
}

void Trigger::SetSourceName(
    Base::StringView value) noexcept {
    SetAuthoredSourceName(authored_, value);
}

void Trigger::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    SetAuthoredPropertyValue(authored_, value);
}

// ===== SetterTriggerBase =====

void SetterTriggerBase::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(std::move(setter));
}

// ===== DataTrigger =====

DataTrigger::~DataTrigger() {
    ReleaseAuthored();
}

DataTrigger::AuthoredData* DataTrigger::EnsureAuthored() noexcept {
    return EnsureAuthoredData(authored_);
}

void DataTrigger::ReleaseAuthored() noexcept {
    ReleaseAuthoredData(authored_);
}

StringView DataTrigger::GetPropertyName() const noexcept {
    return GetAuthoredPropertyName(authored_);
}

StringView DataTrigger::GetSourceName() const noexcept {
    return GetAuthoredSourceName(authored_);
}

const PropertyValue& DataTrigger::GetAuthoredValue() const noexcept {
    return GetAuthoredPropertyValue(authored_);
}

void DataTrigger::SetPropertyName(StringView value) noexcept {
    SetAuthoredPropertyName(authored_, value);
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    SetAuthoredSourceName(authored_, value);
}

void DataTrigger::SetAuthoredValue(const PropertyValue& value) noexcept {
    SetAuthoredPropertyValue(authored_, value);
}

// ===== MultiTrigger =====

void MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(std::move(condition));
}

// ===== MultiDataTrigger =====

void MultiDataTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) { AERO_ASSERT(false); return; }
    conditions_.PushBack(
        std::move(condition));
}

// ===== Condition =====

Condition::~Condition() {
    ReleaseAuthored();
}

Condition::AuthoredData* Condition::EnsureAuthored() noexcept {
    return EnsureAuthoredData(authored_);
}

void Condition::ReleaseAuthored() noexcept {
    ReleaseAuthoredData(authored_);
}

StringView Condition::GetPropertyName() const noexcept {
    return GetAuthoredPropertyName(authored_);
}

StringView Condition::GetSourceName() const noexcept {
    return GetAuthoredSourceName(authored_);
}

const PropertyValue& Condition::GetAuthoredValue() const noexcept {
    return GetAuthoredPropertyValue(authored_);
}

void Condition::SetAuthoredValue(const PropertyValue& value) noexcept {
    SetAuthoredPropertyValue(authored_, value);
}

void Condition::SetPropertyName(
    Base::StringView value) noexcept {
    SetAuthoredPropertyName(authored_, value, true);
}

void Condition::SetSourceName(
    Base::StringView value) noexcept {
    SetAuthoredSourceName(authored_, value);
}

} // namespace Aero

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
namespace {

void AddDataTriggerContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& trigger = static_cast<DataTrigger&>(owner);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        Base::Ref<Setter> setter = Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
        if (setter) trigger.AddAuthoredSetter(std::move(setter));
        return;
    }
    trigger.AddEnterAction(value);
}

void ClearDataTriggerContent(Base::Object& owner, void*) noexcept {
    auto& trigger = static_cast<DataTrigger&>(owner);
    trigger.ClearAuthoredSetters();
    trigger.ClearEnterActions();
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero {

AERO_DESCRIBE(SetterBase) {
    using namespace Aero::Meta;
    Register<SetterBase>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(Setter) {
    using namespace Aero::Meta;
    Register<Setter>(context)
            .Property("TargetName", &Setter::GetTargetName, &Setter::SetTargetName)
            .Property("Property", &Setter::GetPropertyName, &Setter::SetPropertyName)
            .Property<Value, &Setter::GetAuthoredValue, &Setter::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(EventSetter) {
    using namespace Aero::Meta;
    Register<EventSetter>(context)
            .Property("Handler", &EventSetter::GetHandlerName, &EventSetter::SetHandlerName)
            .Factory();
}

AERO_DESCRIBE(TriggerBase) {
    using namespace Aero::Meta;
    Register<TriggerBase>(context, TypeFlags::Abstract)
            .Collection<Aero::Interactivity::TriggerAction, &TriggerBase::AddEnterAction, &TriggerBase::ClearEnterActions>("EnterActions")
            .Collection<Aero::Interactivity::TriggerAction, &TriggerBase::AddExitAction, &TriggerBase::ClearExitActions>("ExitActions");
}

AERO_DESCRIBE(SetterTriggerBase) {
    using namespace Aero::Meta;
    Register<SetterTriggerBase>(context, TypeFlags::Abstract)
            .Content<Setter, &SetterTriggerBase::AddAuthoredSetter, &SetterTriggerBase::ClearAuthoredSetters>("Setters");
}

AERO_DESCRIBE(Trigger) {
    using namespace Aero::Meta;
    Register<Trigger>(context)
            .Property("Property", &Trigger::GetPropertyName, &Trigger::SetPropertyName)
            .Property("SourceName", &Trigger::GetSourceName, &Trigger::SetSourceName)
            .Property<Value, &Trigger::GetAuthoredValue, &Trigger::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Content<Setter, &SetterTriggerBase::AddAuthoredSetter, &SetterTriggerBase::ClearAuthoredSetters>("Setters")
            .Factory();
}

AERO_DESCRIBE(DataTrigger) {
    using namespace Aero::Meta;
    Register<DataTrigger>(context)
            .Property<Base::Ref<Data::Binding>, &DataTrigger::SetBinding>("Binding", PropertyFlags::Structural)
            .Property<Value, &DataTrigger::GetAuthoredValue, &DataTrigger::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Property("Comparison", &DataTrigger::GetComparison, &DataTrigger::SetComparison)
            .Content<Base::Object>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddDataTriggerContent, &::Aero::MetadataSupport::ClearDataTriggerContent)
            .Factory();
}

AERO_DESCRIBE(Condition) {
    using namespace Aero::Meta;
    Register<Condition>(context)
            .Property<Base::Ref<Data::Binding>, &Condition::SetBinding>("Binding", PropertyFlags::Structural)
            .Property("Property", &Condition::GetPropertyName, &Condition::SetPropertyName)
            .Property("SourceName", &Condition::GetSourceName, &Condition::SetSourceName)
            .Property<Value, &Condition::GetAuthoredValue, &Condition::SetAuthoredValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(MultiDataTrigger) {
    using namespace Aero::Meta;
    Register<MultiDataTrigger>(context)
            .Collection<Condition, &MultiDataTrigger::AddCondition, &MultiDataTrigger::ClearConditions>("Conditions")
            .Content<Setter, &SetterTriggerBase::AddAuthoredSetter, &SetterTriggerBase::ClearAuthoredSetters>("Setters")
            .Factory();
}

AERO_DESCRIBE(MultiTrigger) {
    using namespace Aero::Meta;
    Register<MultiTrigger>(context)
            .Collection<Condition, &MultiTrigger::AddCondition, &MultiTrigger::ClearConditions>("Conditions")
            .Content<Setter, &SetterTriggerBase::AddAuthoredSetter, &SetterTriggerBase::ClearAuthoredSetters>("Setters")
            .Factory();
}

} // namespace Aero


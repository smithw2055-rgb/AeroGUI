#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Events.hpp>
#include <Aero/Value.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>

#include "gui/core/Describe.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/TryCast.hpp>

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

bool ResolveTriggerSetterValue(
    DependencyObject& target,
    DependencyPropertyHandle property,
    const PropertyValue& rawValue,
    PropertyValue& resolvedValue,
    FrameworkElement* fallbackScope,
    const ResourceDictionary* fallbackResources,
    bool fallbackToDefaultOnMissingResource) noexcept {
    Base::StringView dynamicKey;
    if (IsDynamicResourceSetterValue(rawValue, dynamicKey)) {
        resolvedValue = rawValue;
        auto* element = ::Aero::TryCast<FrameworkElement>(&target);
        ResourceValue lookedUp{};
        if (element != nullptr) {
            Base::Result<ResourceValue> res = element->FindResource(dynamicKey);
            if (res) lookedUp = res.Value();
        }
        if (lookedUp.IsUnset() && fallbackScope != nullptr) {
            Base::Result<ResourceValue> res = fallbackScope->FindResource(dynamicKey);
            if (res) lookedUp = res.Value();
        }
        if (lookedUp.IsUnset() && fallbackResources != nullptr) {
            Base::Result<ResourceValue> res = fallbackResources->Lookup(dynamicKey);
            if (res) lookedUp = res.Value();
        }
        const DependencyProperty* prop =
            DependencyObjectAccess::PropertyRegistry(target).Find(property);
        if (!lookedUp.IsUnset()) {
            if (prop != nullptr) {
                Base::Result<PropertyValue> converted =
                    Markup::ConvertDynamicResourceValue(lookedUp, *prop);
                if (converted) {
                    resolvedValue = std::move(converted).Value();
                } else {
                    resolvedValue = lookedUp;
                }
            } else {
                resolvedValue = lookedUp;
            }
        } else if (fallbackToDefaultOnMissingResource && prop != nullptr) {
            const Meta::PropertyMetadata* meta =
                prop->MetadataFor(target.RuntimeType());
            if (meta == nullptr) {
                meta = prop->MetadataFor(prop->RegisteredOwnerType());
            }
            if (meta != nullptr && !meta->defaultValue.IsUnset()) {
                resolvedValue = meta->defaultValue;
            }
        }
        return true;
    }
    if (IsDeferredBindingSetterValue(rawValue)) {
        return false;
    }
    resolvedValue = rawValue;
    return true;
}

struct TriggerBase::ActionStorage {
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
    Base::Vector<Base::Ref<Base::Object>> behaviors;
};

TriggerBase::~TriggerBase() {
    delete actionStorage_;
    actionStorage_ = nullptr;
}

TriggerBase::ActionStorage* TriggerBase::EnsureActions() noexcept {
    if (actionStorage_ == nullptr) {
        actionStorage_ = new (std::nothrow) ActionStorage();
    }
    return actionStorage_;
}

void TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    if (ActionStorage* storage = EnsureActions()) {
        storage->enterActions.PushBack(std::move(action));
    }
}

void TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) { AERO_ASSERT(false); return; }
    if (ActionStorage* storage = EnsureActions()) {
        storage->exitActions.PushBack(std::move(action));
    }
}

void TriggerBase::ClearEnterActions() noexcept {
    if (actionStorage_ != nullptr) {
        actionStorage_->enterActions.Clear();
    }
}

void TriggerBase::ClearExitActions() noexcept {
    if (actionStorage_ != nullptr) {
        actionStorage_->exitActions.Clear();
    }
}

Span<const Ref<Base::Object>> TriggerBase::GetEnterActions() const noexcept {
    if (actionStorage_ == nullptr) return {};
    return {actionStorage_->enterActions.Data(), actionStorage_->enterActions.Size()};
}

Span<const Ref<Base::Object>> TriggerBase::GetExitActions() const noexcept {
    if (actionStorage_ == nullptr) return {};
    return {actionStorage_->exitActions.Data(), actionStorage_->exitActions.Size()};
}

void TriggerBase::AddBehavior(Ref<Base::Object> behavior) noexcept {
    if (!behavior) { AERO_ASSERT(false); return; }
    if (ActionStorage* storage = EnsureActions()) {
        storage->behaviors.PushBack(std::move(behavior));
    }
}

void TriggerBase::ClearBehaviors() noexcept {
    if (actionStorage_ != nullptr) {
        actionStorage_->behaviors.Clear();
    }
}

Span<const Ref<Base::Object>> TriggerBase::GetBehaviors() const noexcept {
    if (actionStorage_ == nullptr) return {};
    return {actionStorage_->behaviors.Data(), actionStorage_->behaviors.Size()};
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

void Trigger::SetPropertyName(Base::StringView value) noexcept {
    if (value.Empty()) return;
    static_cast<void>(propertyName_.Assign(value));
}

void Trigger::SetSourceName(Base::StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(::Aero::Base::ValueConversion::Trim(value)));
}

void Trigger::SetAuthoredValue(const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    value_ = value;
}

// ===== SetterTriggerBase =====

void SetterTriggerBase::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetters_.PushBack(std::move(setter));
}

// ===== DataTrigger =====

void DataTrigger::SetPropertyName(StringView value) noexcept {
    if (value.Empty()) return;
    static_cast<void>(propertyName_.Assign(value));
}

void DataTrigger::SetSourceName(StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(::Aero::Base::ValueConversion::Trim(value)));
}

void DataTrigger::SetAuthoredValue(const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
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

void Condition::SetAuthoredValue(const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
}

void Condition::SetPropertyName(Base::StringView value) noexcept {
    if (value.Empty()) return;
    static_cast<void>(propertyName_.Assign(::Aero::Base::ValueConversion::Trim(value)));
}

void Condition::SetSourceName(Base::StringView value) noexcept {
    static_cast<void>(sourceName_.Assign(::Aero::Base::ValueConversion::Trim(value)));
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
            .Property<&Setter::GetTargetName, &Setter::SetTargetName>("TargetName")
            .Property<&Setter::GetPropertyName, &Setter::SetPropertyName>("Property")
            .Property<&Setter::GetAuthoredValue, &Setter::SetAuthoredValue>("Value")
            .Factory();
}

AERO_DESCRIBE(EventSetter) {
    using namespace Aero::Meta;
    Register<EventSetter>(context)
            .Property<&EventSetter::GetHandlerName, &EventSetter::SetHandlerName>("Handler")
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
            .Property<&Trigger::GetPropertyName, &Trigger::SetPropertyName>("Property")
            .Property<&Trigger::GetSourceName, &Trigger::SetSourceName>("SourceName")
            .Property<&Trigger::GetAuthoredValue, &Trigger::SetAuthoredValue>("Value")
            .Content<Setter, &SetterTriggerBase::AddAuthoredSetter, &SetterTriggerBase::ClearAuthoredSetters>("Setters")
            .Factory();
}

AERO_DESCRIBE(DataTrigger) {
    using namespace Aero::Meta;
    Register<DataTrigger>(context)
            .Property<&DataTrigger::SetBinding>("Binding", PropertyFlags::Structural)
            .Property<&DataTrigger::GetAuthoredValue, &DataTrigger::SetAuthoredValue>("Value")
            .Property<&DataTrigger::GetComparison, &DataTrigger::SetComparison>("Comparison")
            .Content<Base::Object>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddDataTriggerContent, &::Aero::MetadataSupport::ClearDataTriggerContent)
            .Factory();
}

AERO_DESCRIBE(Condition) {
    using namespace Aero::Meta;
    Register<Condition>(context)
            .Property<&Condition::SetBinding>("Binding", PropertyFlags::Structural)
            .Property<&Condition::GetPropertyName, &Condition::SetPropertyName>("Property")
            .Property<&Condition::GetSourceName, &Condition::SetSourceName>("SourceName")
            .Property<&Condition::GetAuthoredValue, &Condition::SetAuthoredValue>("Value")
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


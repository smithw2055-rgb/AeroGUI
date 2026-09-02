#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"
#include "gui/triggers/TriggerEngine.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Style.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Triggers/Triggers.hpp>
#include <Aero/Value.hpp>
#include <Aero/UIElement.hpp>

#include <new>

namespace Aero {

void Element::OnBlendingModeChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (!object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(), UIElement::StaticTypeId())) {
        return;
    }
    Base::Result<BlendMode> value =
        Meta::ValueCodec<BlendMode>::Decode(args.GetNewValue());
    if (!value) return;
    static_cast<UIElement&>(object).SetBlendMode(value.Value());
}

void Element::OnTransform3DChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    UIElement* element = ::Aero::TryCast<UIElement>(&object);
    if (element == nullptr) return;
    Base::Result<Base::Ref<Media::Transform3D>> value =
        element->GetValue(Element::Transform3DProperty);
    element->SetTransform3D(
        value ? std::move(value).Value() : Base::Ref<Media::Transform3D>{});
}

void TextProperties::OnCompatibilityPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const Meta::DependencyProperty* source =
        object.PropertyRegistry().Find(args.GetProperty());
    if (source == nullptr) return;

    const Meta::PropertyInfo* targetInfo =
        object.PropertyRegistry().Types().FindProperty(
            object.RuntimeType(), source->Name(), false);
    if (targetInfo == nullptr ||
        targetInfo->Id() == source->Handle().value) {
        return;
    }
    const Meta::DependencyProperty* target =
        object.PropertyRegistry().Find(
            Meta::DependencyPropertyHandle{targetInfo->Id()});
    if (target == nullptr ||
        target->MetadataFor(object.RuntimeType()) == nullptr) {
        return;
    }

    Meta::Value value = args.GetNewValue();
    if (!target->AcceptsAnyValue() &&
        value.Type() != target->ValueType() &&
        value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject() &&
        object.PropertyRegistry().Types().IsDerivedFrom(
            value.AsObject()->RuntimeType(), target->ValueType())) {
        value = Meta::Value::FromObject(
            target->ValueType(), value.AsObject());
    }
    (void)object.SetValueChecked(target->Handle(), value);
}

std::uint32_t SetterBaseCollection::GetCount() const noexcept {
    return owner_ != nullptr ? owner_->GetAuthoredSetters().Size() : 0U;
}

SetterBase* SetterBaseCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr || index >= owner_->GetAuthoredSetters().Size()) return nullptr;
    return owner_->GetAuthoredSetters()[index].Get();
}

Base::Result<void> SetterBaseCollection::Add(
    Base::Ref<SetterBase> setter) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Setter collection is detached");
    }
    return owner_->AddAuthoredSetter(std::move(setter));
}

Base::Result<void> SetterBaseCollection::Add(
    Base::Ref<Setter> setter) noexcept {
    return Add(Base::Ref<SetterBase>(std::move(setter)));
}

void SetterBaseCollection::Clear() noexcept {
    if (owner_ != nullptr) static_cast<void>(owner_->ClearAuthoredSetters());
}

std::uint32_t TriggerCollection::GetCount() const noexcept {
    return owner_ != nullptr ? owner_->GetAuthoredTriggers().Size() : 0U;
}

TriggerBase* TriggerCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr || index >= owner_->GetAuthoredTriggers().Size()) return nullptr;
    return owner_->GetAuthoredTriggers()[index].Get();
}

Base::Result<void> TriggerCollection::Add(
    Base::Ref<TriggerBase> trigger) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Trigger collection is detached");
    }
    return owner_->AddAuthoredTrigger(std::move(trigger));
}

void TriggerCollection::Clear() noexcept {
    if (owner_ != nullptr) static_cast<void>(owner_->ClearAuthoredTriggers());
}


using namespace Aero::Meta;
using namespace Aero::Threading;
namespace {


bool IsTargetCompatible(
    const TypeRegistry& types,
    const DependencyPropertyRegistry* properties,
    TypeId derived,
    TypeId expectedBase) noexcept {
    if (derived == expectedBase ||
        types.IsDerivedFrom(derived, expectedBase)) {
        return true;
    }
    // Some container controls retain a separate implementation base for
    // generator ownership, while exposing the complete WPF item contracts.
    // Accept those explicit metadata contracts as style bases without
    // pretending that the C++ inheritance graph is different.
    const TypeInfo* expected = types.FindType(expectedBase);
    if (properties == nullptr || expected == nullptr) return false;
    if (expected->Name() ==
            Base::StringView("HeaderedItemsControl")) {
        return properties->Find(derived, "Header") != nullptr &&
            properties->Find(derived, "HeaderTemplate") != nullptr;
    }
    if (expected->Name() == Base::StringView("ItemsControl")) {
        return properties->Find(derived, "ItemsSource") != nullptr &&
            properties->Find(derived, "ItemTemplate") != nullptr;
    }
    // Headered items controls (e.g. HeaderedItemsControl, TreeViewItem) expose
    // the WPF item contract through Header/HeaderTemplate rather than
    // ItemsSource/ItemTemplate, yet still derive from the items-control
    // family. Accept them as valid ContentControl-based styles too.
    if (expected->Name() == Base::StringView("ContentControl")) {
        return properties->Find(derived, "Header") != nullptr &&
            properties->Find(derived, "HeaderTemplate") != nullptr;
    }
    return false;
}

Base::Result<PropertyValue> NormalizeStyleValue(
    const DependencyProperty&,
    const PropertyValue& value) noexcept {
    return value;
}

} // namespace


Base::Result<void> StyleState::Freeze(
    TypeId valueTargetType,
    Base::Vector<StyleSetter>&& valueSetters,
    Base::Vector<TriggerPlan>&& valueTriggers) noexcept {
    if (frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "StyleProgram is already frozen");
    }
    if (valueTargetType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StyleProgram target type is invalid");
    }
    targetType = valueTargetType;
    setters = std::move(valueSetters);
    triggers = std::move(valueTriggers);
    frozen = true;
    return {};
}

void StyleState::Reset() noexcept {
    targetType = InvalidTypeId;
    setters.Clear();
    triggers.Clear();
    frozen = false;
}

Base::Result<void> StyleState::AddAuthoredSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    for (const StyleSetter& setter : authoredSetters) {
        if (setter.property == property) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Style already has a setter for this property");
        }
    }
    return authoredSetters.PushBack({property, value});
}

Base::Result<void> StyleState::AddAuthoredTrigger(
    TriggerPlan trigger) noexcept {
    return authoredTriggers.PushBack(std::move(trigger));
}

void StyleState::ClearAuthored() noexcept {
    authoredSetters.Clear();
    authoredTriggers.Clear();
}

void Setter::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return;
    }
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    propertyName_ = std::move(candidate);
}

void Setter::SetTargetName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return;
    }
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    targetName_ = std::move(candidate);
}

void Setter::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
}

Base::Result<void> Setter::Resolve(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (!property.IsValid() || value.IsUnset()) {
        return InvalidStyle("Setter resolve value is invalid");
    }
    SetProperty(property);
    SetValue(value);
    return {};
}

Style::Style() noexcept
    : Style(InvalidTypeId, nullptr) {}

Style::Style(
    TypeId targetType,
    const Style* basedOn) noexcept
    : Style(
          targetType,
          basedOn,
          StaticTypeId()) {}

Style::Style(
    TypeId targetType,
    const Style* basedOn,
    TypeId runtimeType) noexcept
    : runtimeType_(runtimeType),
      targetType_(targetType),
      basedOn_(basedOn),
      implAllocator_(&Base::GetDefaultAllocator()),
      resources_() {
    void* memory = implAllocator_->Allocate({
        sizeof(StyleState), alignof(StyleState), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(StyleState), alignof(StyleState), Base::MemoryTag::Ui);
    }
    program_ = new (memory) StyleState{};
}

Style::~Style() {
    if (program_ == nullptr) return;
    program_->~StyleState();
    implAllocator_->Deallocate(
        program_, sizeof(StyleState), alignof(StyleState), Base::MemoryTag::Ui);
    program_ = nullptr;
}

TypeId Style::GetTargetType() const noexcept {
    return sealed_ && program_ != nullptr
        ? program_->TargetType()
        : targetType_;
}

bool Style::SetTargetType(TypeId targetType) noexcept {
    if (sealed_) {
        return false;
    }
    if (targetType == InvalidTypeId) {
        return false;
    }
    targetType_ = targetType;
    return true;
}

bool Style::SetBasedOn(const Style* basedOn) noexcept {
    if (sealed_) {
        return false;
    }
    if (basedOn == this) {
        return false;
    }
    basedOn_ = basedOn;
    return true;
}

Base::Result<void> Style::AddSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!property.IsValid() || value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style setter requires a property and concrete value");
    }
    return program_->AddAuthoredSetter(property, value);
}

bool Style::SetBasedOn(
    Base::Ref<Base::Object> basedOn) noexcept {
    if (basedOn &&
        basedOn->RuntimeType() != RuntimeType()) {
        return false;
    }
    if (!SetBasedOn(static_cast<Style*>(basedOn.Get()))) return false;
    basedOnOwner_ = std::move(basedOn);
    return true;
}

Base::Result<void> Style::AddAuthoredSetter(
    Base::Ref<SetterBase> setter) noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    if (!setter) {
        return InvalidStyle(
            "Style authored setter is null");
    }
    return authoredSetterObjects_.PushBack(
        std::move(setter));
}

Base::Result<void> Style::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    return AddAuthoredSetter(Base::Ref<SetterBase>(std::move(setter)));
}

Base::Result<void> Style::AddAuthoredTrigger(
    Base::Ref<TriggerBase> trigger) noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    if (!trigger) {
        return InvalidStyle(
            "Style authored trigger is null");
    }
    return authoredTriggerObjects_.PushBack(
        std::move(trigger));
}

void Style::ClearAuthoredSetters() noexcept {
    if (sealed_) {
        return;
    }
    authoredSetterObjects_.Clear();
}

void EventSetter::SetHandlerName(Base::StringView value) noexcept {
    static_cast<void>(handlerName_.Assign(value));
}

void Style::ClearAuthoredTriggers() noexcept {
    if (sealed_) {
        return;
    }
    authoredTriggerObjects_.Clear();
}

Base::Result<void> Style::AddSetter(
    const Setter& setter) noexcept {
    return AddSetter(
        setter.GetProperty(), setter.GetValue());
}

Base::Result<void> Style::AddPropertyTrigger(
    DependencyPropertyHandle condition,
    const PropertyValue& conditionValue,
    DependencyPropertyHandle property,
    PropertyValue value) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!condition.IsValid() || conditionValue.IsUnset() ||
        !property.IsValid() || value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style property trigger is incomplete");
    }
    TriggerPlan trigger;
    trigger.property = condition;
    trigger.value = conditionValue;
    Base::Result<void> setter = trigger.setters.PushBack(
        {property, std::move(value)});
    if (!setter) return setter.GetStatus();
    return program_->AddAuthoredTrigger(std::move(trigger));
}

Base::Result<void> Style::AddTrigger(
    const Trigger& trigger) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!trigger.property_.IsValid() || trigger.value_.IsUnset() ||
        trigger.setterProperties_.Size() != trigger.setterValues_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Trigger is incomplete");
    }
    TriggerPlan plan;
    plan.property = trigger.property_;
    plan.value = trigger.value_;
    for (std::uint32_t index = 0U;
         index < trigger.setterProperties_.Size(); ++index) {
        Base::Result<void> copied = plan.setters.PushBack({
            trigger.setterProperties_[index], trigger.setterValues_[index]});
        if (!copied) return copied.GetStatus();
    }
    Base::Result<void> copied = plan.enterActions.Append(
        trigger.GetEnterActions());
    if (!copied) return copied.GetStatus();
    copied = plan.exitActions.Append(trigger.GetExitActions());
    if (!copied) return copied.GetStatus();
    return program_->AddAuthoredTrigger(std::move(plan));
}

Base::Result<void> Style::AddTrigger(
    const DataTrigger& trigger) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!trigger.GetBinding() || trigger.GetAuthoredValue().IsUnset() ||
        trigger.GetAuthoredSetters().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTrigger is incomplete");
    }
    TriggerPlan plan;
    plan.binding = trigger.GetBinding();
    plan.value = trigger.GetAuthoredValue();
    for (const Base::Ref<Setter>& authored :
         trigger.GetAuthoredSetters()) {
        if (!authored || !authored->GetProperty().IsValid() ||
            authored->GetValue().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTrigger Setter is incomplete");
        }
        Base::Result<void> copied = plan.setters.PushBack({
            authored->GetProperty(), authored->GetValue()});
        if (!copied) return copied.GetStatus();
    }
    Base::Result<void> copied = plan.enterActions.Append(
        trigger.GetEnterActions());
    if (!copied) return copied.GetStatus();
    copied = plan.exitActions.Append(trigger.GetExitActions());
    if (!copied) return copied.GetStatus();
    return program_->AddAuthoredTrigger(std::move(plan));
}

Base::Result<void> Style::AddTrigger(
    const MultiDataTrigger& trigger) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (trigger.GetConditions().Empty() ||
        trigger.GetAuthoredSetters().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MultiDataTrigger is incomplete");
    }
    TriggerPlan plan;
    bool first = true;
    for (const Base::Ref<Condition>& condition :
         trigger.GetConditions()) {
        if (!condition || !condition->GetBinding() ||
            condition->GetAuthoredValue().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "MultiDataTrigger Condition is incomplete");
        }
        if (first) {
            plan.binding = condition->GetBinding();
            plan.value = condition->GetAuthoredValue();
            first = false;
            continue;
        }
        TriggerBindingCondition extra;
        extra.binding = condition->GetBinding();
        extra.value = condition->GetAuthoredValue();
        Base::Result<void> copied =
            plan.extraBindings.PushBack(std::move(extra));
        if (!copied) return copied.GetStatus();
    }
    for (const Base::Ref<Setter>& authored :
         trigger.GetAuthoredSetters()) {
        if (!authored || !authored->GetProperty().IsValid() ||
            authored->GetValue().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "MultiDataTrigger Setter is incomplete");
        }
        Base::Result<void> copied = plan.setters.PushBack({
            authored->GetProperty(), authored->GetValue()});
        if (!copied) return copied.GetStatus();
    }
    Base::Result<void> copied = plan.enterActions.Append(
        trigger.GetEnterActions());
    if (!copied) return copied.GetStatus();
    copied = plan.exitActions.Append(trigger.GetExitActions());
    if (!copied) return copied.GetStatus();
    return program_->AddAuthoredTrigger(std::move(plan));
}

Base::Result<void> Style::SealRuntime(
    const void* propertiesState) noexcept {
    if (propertiesState == nullptr) {
        return InvalidStyle("Style has no dependency-property registry");
    }
    const auto& properties = *static_cast<
        const DependencyPropertyRegistry*>(propertiesState);
    if (sealed_) {
        return {};
    }
    // WPF permits an explicitly keyed Style to omit TargetType when its
    // setters use owner-qualified properties. Infer the owner for declarations
    // such as Property="local:DateTime.Template".
    if (targetType_ == InvalidTypeId) {
        for (const Base::Ref<SetterBase>& authored : authoredSetterObjects_) {
            Setter* setter = ::Aero::TryCast<Setter>(authored.Get());
            if (setter == nullptr) continue;
            Base::StringView name = setter->GetPropertyName();
            std::uint32_t dot = UINT32_MAX;
            for (std::uint32_t index = 0U;
                 index < name.SizeBytes(); ++index) {
                if (name[index] == '.') dot = index;
            }
            if (dot == UINT32_MAX || dot == 0U) continue;
            Base::StringView owner = name.Substr(0U, dot);
            const Base::StringView propertyName = name.Substr(
                dot + 1U, name.SizeBytes() - dot - 1U);
            for (std::uint32_t index = 0U;
                 index < owner.SizeBytes(); ++index) {
                if (owner[index] == ':') {
                    owner = owner.Substr(
                        index + 1U, owner.SizeBytes() - index - 1U);
                    break;
                }
            }
            for (const TypeInfo& type : properties.Types().Types()) {
                if (type.Name() == owner &&
                    properties.Find(type.Id(), propertyName) != nullptr) {
                    targetType_ = type.Id();
                    break;
                }
            }
            if (targetType_ != InvalidTypeId) break;
        }
    }
    if (!properties.IsFrozen() || targetType_ == InvalidTypeId ||
        properties.Types().FindType(targetType_) == nullptr) {
        return InvalidStyle("Style requires a frozen registry and registered target type");
    }

    const Style* ancestor = basedOn_;
    while (ancestor != nullptr) {
        if (ancestor == this) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Style BasedOn graph contains a cycle");
        }
        if (!ancestor->sealed_) {
            return InvalidStyle("BasedOn style must be sealed before its derived style");
        }
        if (!IsTargetCompatible(
                properties.Types(), &properties,
                targetType_, ancestor->targetType_)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Derived Style target type is incompatible with BasedOn target type");
        }
        ancestor = ancestor->basedOn_;
    }

    Base::Vector<StyleSetter> next;
    if (basedOn_ != nullptr) {
        Base::Result<void> inherited = next.Append(
            StyleState::RuntimeSetters(*basedOn_));
        if (!inherited) {
            return inherited.GetStatus();
        }
    }
    for (const StyleSetter& setter : program_->authoredSetters) {
        const Meta::DependencyProperty* property =
            properties.Find(setter.property);
        if (property == nullptr || property->MetadataFor(targetType_) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Style setter does not apply to its target type");
        }
        PropertyValue normalizedValue = setter.value;
        if (!IsDeferredBindingSetterValue(normalizedValue)) {
            Base::Result<PropertyValue> normalized =
                NormalizeStyleValue(*property, normalizedValue);
            if (!normalized) return normalized.GetStatus();
            normalizedValue = std::move(normalized).Value();
            Base::Result<void> validValue = properties.ValidateValueFor(
                setter.property, targetType_, normalizedValue);
            if (!validValue) {
                return validValue.GetStatus();
            }
        }
        bool replaced = false;
        for (StyleSetter& inherited : next) {
            if (inherited.property == setter.property) {
                inherited.value = normalizedValue;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            Base::Result<void> appended = next.PushBack({
                setter.property, normalizedValue});
            if (!appended) {
                return appended.GetStatus();
            }
        }
    }
    Base::Vector<TriggerPlan> nextTriggers;
    if (basedOn_ != nullptr) {
        Base::Result<void> inherited =
            nextTriggers.Append(StyleState::RuntimeTriggers(*basedOn_));
        if (!inherited) return inherited.GetStatus();
    }
    for (const TriggerPlan& trigger : program_->authoredTriggers) {
        if (trigger.IsBindingTrigger()) {
            if (!trigger.binding || trigger.value.IsUnset()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger Binding or Value is incomplete");
            }
            for (const TriggerBindingCondition& extra :
                 trigger.extraBindings) {
                if (!extra.binding || extra.value.IsUnset()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style MultiDataTrigger Condition is incomplete");
                }
            }
        } else {
            const Meta::DependencyProperty* condition =
                properties.Find(trigger.property);
            if (condition == nullptr ||
                condition->MetadataFor(targetType_) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style trigger condition does not apply to TargetType");
            }
            Base::Result<void> validCondition = properties.ValidateValueFor(
                trigger.property, targetType_, trigger.value);
            if (!validCondition) return validCondition.GetStatus();
        }
        for (std::uint32_t index = 0U;
             index < trigger.setters.Size();
             ++index) {
            const StyleTriggerSetter& setter = trigger.setters[index];
            const Meta::DependencyProperty* property =
                properties.Find(setter.property);
            if (property == nullptr ||
                property->MetadataFor(targetType_) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style trigger setter does not apply to TargetType");
            }
            if (!IsDeferredBindingSetterValue(setter.value)) {
                Base::Result<void> validValue = properties.ValidateValueFor(
                    setter.property, targetType_, setter.value);
                if (!validValue) return validValue.GetStatus();
            }
            for (std::uint32_t previous = 0U;
                 previous < index;
                 ++previous) {
                if (trigger.setters[previous].property ==
                    setter.property) {
                    return Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        "Style trigger repeats a setter property");
                }
            }
        }
        Base::Result<void> appended =
            nextTriggers.PushBack(trigger);
        if (!appended) return appended.GetStatus();
    }
    Base::Result<void> frozenProgram = program_->Freeze(
        targetType_, std::move(next), std::move(nextTriggers));
    if (!frozenProgram) return frozenProgram.GetStatus();
    Base::Result<void> sealedResources = resources_.Seal();
    if (!sealedResources) {
        program_->Reset();
        return sealedResources.GetStatus();
    }
    program_->ClearAuthored();
    authoredSetterObjects_.Clear();
    // Retain immutable EventTrigger declarations for per-element routed-event
    // subscriptions. Property/DataTrigger plans are compiled into program_.
    // Keep the BasedOn link so callers can still query the resolved base
    // style after sealing (GetBasedOn()).
    sealed_ = true;
    return {};
}

void Style::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::AssignResourceDictionary(
        resources_,
        std::move(value),
        "Style Resources is already assigned");
}

Base::Result<void> StyleState::Seal(
    Style& style,
    const void* properties) noexcept {
    return style.SealRuntime(properties);
}

Base::Span<const StyleSetter> StyleState::RuntimeSetters(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Setters()
        : Base::Span<const StyleSetter>{};
}

Base::Span<const TriggerPlan> StyleState::RuntimeTriggers(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Triggers()
        : Base::Span<const TriggerPlan>{};
}

} // namespace Aero

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero;

Base::Result<void> StyleEngine::VerifyTarget(
    const DependencyObject& object,
    const Style& style) const noexcept {
    if (values_ == nullptr || properties_ == nullptr || !style.GetIsSealed()) {
        return InvalidStyle("StyleEngine requires a sealed Style");
    }
    if (!IsTargetCompatible(
            properties_->Types(), properties_, object.RuntimeType(),
            style.GetTargetType())) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style target type is incompatible with the object type");
    }
    return {};
}

Base::Result<void> StyleEngine::Apply(
    DependencyObject& object,
    const Style& style) noexcept {
    Base::Result<void> hooked =
        triggerEngine_->EnsureTriggerPhaseHook(object);
    if (!hooked) return hooked.GetStatus();
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    const std::uint32_t existing = FindApplication(object);
    const bool requiresSubscription =
        existing == UINT32_MAX ||
        applications_[existing].style != &style;
    if (existing != UINT32_MAX && applications_[existing].style != &style) {
        triggerEngine_->RemovePendingTriggerEvaluation(object);
        triggerEngine_->UnsubscribeTriggers(
            object, *applications_[existing].style);
        Base::Result<void> triggers = triggerEngine_->ClearTriggerSetters(
            object, *applications_[existing].style);
        if (!triggers) return triggers.GetStatus();
        Base::Result<void> cleared = ClearSetters(
            object, *applications_[existing].style);
        if (!cleared) {
            return cleared.GetStatus();
        }
    }
    for (const StyleSetter& setter : StylePrivate::RuntimeSetters(style)) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> applied = values_->SetStyleValue(
            object, setter.property, setter.value);
        if (!applied) {
            return applied.GetStatus();
        }
    }
    UIElement* element = ::Aero::TryCast<UIElement>(&object);
    if (element != nullptr) {
        for (const Base::Ref<SetterBase>& authored : style.GetAuthoredSetters()) {
            EventSetter* eventSetter =
                ::Aero::TryCast<EventSetter>(authored.Get());
            if (eventSetter == nullptr ||
                !eventSetter->GetEvent().IsValid() ||
                eventSetter->GetHandler().Empty()) {
                continue;
            }
            Base::Result<void> added = element->AddHandlerChecked(
                eventSetter->GetEvent(),
                eventSetter->GetHandler());
            if (!added) return added.GetStatus();
        }
    }
    if (existing == UINT32_MAX) {
        StyleApplication application;
        application.object = &object;
        application.style = &style;
        Base::Result<void> states =
            application.triggerStates.Resize(
                StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (states) states = application.bindingTriggerStates.Resize(
            StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (states) states = application.bindingTriggerKnown.Resize(
            StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (!states) return states.GetStatus();
        Base::Result<void> tracked =
            applications_.PushBack(
                std::move(application));
        if (!tracked) {
            return tracked.GetStatus();
        }
    } else if (requiresSubscription) {
        applications_[existing].style = &style;
        Base::Result<void> states =
            applications_[existing].triggerStates.Resize(
                StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (states) states = applications_[existing].bindingTriggerStates.Resize(
            StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (states) states = applications_[existing].bindingTriggerKnown.Resize(
            StylePrivate::RuntimeTriggers(style).Size(), 0U);
        if (!states) return states.GetStatus();
    }
    if (requiresSubscription) {
        Base::Result<void> attached = AttachSetterBindings(object, style);
        if (!attached) return attached.GetStatus();
        Base::Result<void> subscribed =
            triggerEngine_->SubscribeTriggers(object, style);
        if (!subscribed) return subscribed.GetStatus();
    }
    return triggerEngine_->EvaluateTriggers(object, style);
}

Base::Result<void> StyleEngine::SetBindingTriggerState(
    DependencyObject& object,
    const Style& style,
    std::uint32_t triggerIndex,
    bool active) noexcept {
    return triggerEngine_->SetBindingTriggerState(
        object, style, triggerIndex, active);
}

Base::Result<void> StyleEngine::Clear(
    DependencyObject& object,
    const Style& style) noexcept {
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    const std::uint32_t existing = FindApplication(object);
    const Style* actual = existing != UINT32_MAX ? applications_[existing].style : &style;
    triggerEngine_->UnsubscribeTriggers(object, *actual);
    Base::Result<void> triggers =
        triggerEngine_->ClearTriggerSetters(object, *actual);
    if (!triggers) return triggers.GetStatus();
    Base::Result<void> cleared = ClearSetters(object, *actual);
    if (!cleared) {
        return cleared.GetStatus();
    }
    if (existing != UINT32_MAX) {
        triggerEngine_->RemovePendingTriggerEvaluation(object);
        if (existing + 1U != applications_.Size()) {
            applications_[existing] = applications_[applications_.Size() - 1U];
        }
        applications_.PopBack();
    }
    return {};
}

Base::Result<bool> StyleEngine::DetachObject(
    DependencyObject& object) noexcept {
    const std::uint32_t existing = FindApplication(object);
    if (existing == UINT32_MAX) {
        return false;
    }
    triggerEngine_->RemovePendingTriggerEvaluation(object);
    triggerEngine_->UnsubscribeTriggers(object, *applications_[existing].style);
    Base::Result<void> triggers =
        triggerEngine_->ClearTriggerSetters(object, *applications_[existing].style);
    if (!triggers) return triggers.GetStatus();
    Base::Result<void> cleared = ClearSetters(object, *applications_[existing].style);
    if (!cleared) {
        return cleared.GetStatus();
    }
    if (existing + 1U != applications_.Size()) {
        applications_[existing] = applications_[applications_.Size() - 1U];
    }
    applications_.PopBack();
    return true;
}

const Style* StyleEngine::AppliedStyle(
    const DependencyObject& object)
    const noexcept {
    const std::uint32_t application =
        FindApplication(object);
    return application != UINT32_MAX
        ? applications_[application].style
        : nullptr;
}

std::uint32_t StyleEngine::FindApplication(
    const DependencyObject& object) const noexcept {
    for (std::uint32_t index = 0U; index < applications_.Size(); ++index) {
        if (applications_[index].object == &object) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<void> StyleEngine::ClearSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    DetachSetterBindings(object);
    for (const StyleSetter& setter : StylePrivate::RuntimeSetters(style)) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> cleared = values_->ClearStyleValue(object, setter.property);
        if (!cleared) {
            return cleared.GetStatus();
        }
    }
    UIElement* element = ::Aero::TryCast<UIElement>(&object);
    if (element != nullptr) {
        for (const Base::Ref<SetterBase>& authored : style.GetAuthoredSetters()) {
            EventSetter* eventSetter =
                ::Aero::TryCast<EventSetter>(authored.Get());
            if (eventSetter == nullptr ||
                !eventSetter->GetEvent().IsValid() ||
                eventSetter->GetHandler().Empty()) {
                continue;
            }
            static_cast<void>(element->RemoveHandler(
                eventSetter->GetEvent(),
                eventSetter->GetHandler()));
        }
    }
    return {};
}

Base::Result<void> StyleEngine::AttachSetterBindings(
    DependencyObject& object,
    const Style& style) noexcept {
    BindingEngine* bindings = AeroGuiInternal::BindingEngineOf(object);
    for (const StyleSetter& setter : StylePrivate::RuntimeSetters(style)) {
        if (!IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        if (bindings == nullptr || bindings->Metadata() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Style Binding setters require a mounted View binding engine");
        }
        Base::Ref<Base::Object> stored = setter.value.AsObject();
        if (!stored || stored->RuntimeType() != Data::Binding::StaticTypeId()) {
            continue;
        }
        auto& binding = static_cast<Data::Binding&>(*stored);
        Data::MetadataBindingDescriptor descriptor;
        descriptor.metadata = bindings->Metadata();
        descriptor.source = binding.GetSource().Get();
        descriptor.target = &object;
        descriptor.targetProperty = setter.property;
        descriptor.dataContextProperty =
            FrameworkElement::DataContextProperty.Handle();
        descriptor.dataContextOwner = &object;
        descriptor.path = binding.GetPathText();
        descriptor.stringFormat = binding.GetStringFormat();
        descriptor.bindsToSource = binding.GetPath().GetIsEmpty();
        descriptor.mode = BindingEngine::ResolveBindingMode(
            object,
            setter.property,
            binding.GetMode());
        descriptor.updateSourceTrigger =
            BindingEngine::ResolveUpdateSourceTrigger(
                object,
                setter.property,
                binding.GetUpdateSourceTrigger());
        descriptor.converterResource = binding.GetConverter();
        descriptor.converterParameter = binding.GetConverterParameter();
        descriptor.fallbackValue = binding.GetFallbackValue();
        descriptor.targetNullValue = binding.GetTargetNullValue();
        Base::Result<Data::BindingHandle> attached =
            bindings->Attach(descriptor);
        if (!attached) return attached.GetStatus();
        Base::Result<void> tracked = setterBindings_.PushBack(
            {&object, attached.Value()});
        if (!tracked) {
            static_cast<void>(bindings->Detach(attached.Value()));
            return tracked.GetStatus();
        }
    }
    return {};
}

void StyleEngine::DetachSetterBindings(DependencyObject& object) noexcept {
    BindingEngine* bindings = AeroGuiInternal::BindingEngineOf(object);
    std::uint32_t keep = 0U;
    for (std::uint32_t index = 0U; index < setterBindings_.Size(); ++index) {
        SetterBinding record = setterBindings_[index];
        if (record.object != &object) {
            setterBindings_[keep] = record;
            ++keep;
            continue;
        }
        if (bindings != nullptr && record.handle.IsValid()) {
            static_cast<void>(bindings->Detach(record.handle));
        }
    }
    static_cast<void>(setterBindings_.Resize(keep));
}

StyleEngine::StyleEngine(
    EffectiveValueEngine& values,
    DependencyPropertyRegistry& properties) noexcept
    : providerSession_(values),
      values_(&providerSession_),
      properties_(&properties),
      applications_(),
      triggerEngine_(new TriggerEngine(
          *values_, *properties_, applications_)) {}

StyleEngine::~StyleEngine() noexcept {
    delete triggerEngine_;
}

void StyleEngine::SetTriggerActionHandler(
    TriggerActionHandler handler, void* context) noexcept {
    triggerEngine_->SetTriggerActionHandler(handler, context);
}

const Base::Status& StyleEngine::LastActionStatus() const noexcept {
    return triggerEngine_->LastActionStatus();
}


} // namespace Aero

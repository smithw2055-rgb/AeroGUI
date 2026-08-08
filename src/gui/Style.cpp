#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include <Aero/Gui/ControlTemplate.hpp>
#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Value.hpp>

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
    Base::Ref<Setter> setter) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Setter collection is detached");
    }
    return owner_->AddAuthoredSetter(std::move(setter));
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

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

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
    return false;
}

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    return value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Data::Binding::StaticTypeId();
}

Base::Result<PropertyValue> NormalizeStyleValue(
    const DependencyProperty&,
    const PropertyValue& value) noexcept {
    return value;
}

} // namespace


Base::Result<void> Style::Access::Freeze(
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

void Style::Access::Reset() noexcept {
    targetType = InvalidTypeId;
    setters.Clear();
    triggers.Clear();
    frozen = false;
}

Base::Result<void> Style::Access::AddAuthoredSetter(
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

Base::Result<void> Style::Access::AddAuthoredTrigger(
    TriggerPlan trigger) noexcept {
    return authoredTriggers.PushBack(std::move(trigger));
}

void Style::Access::ClearAuthored() noexcept {
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

Base::Result<void> TriggerBase::AddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger enter action is null");
    }
    return enterActions_.PushBack(
        std::move(action));
}

Base::Result<void> TriggerBase::AddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger exit action is null");
    }
    return exitActions_.PushBack(
        std::move(action));
}

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
            ::Aero::Base::Detail::ValueConversion::Trim(value))) return;
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

void
Trigger::ClearAuthoredSetters() noexcept {
    authoredSetters_.Clear();
}

Base::Result<void> DataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "DataTrigger authored setter is null");
    }
    return authoredSetters_.PushBack(
        std::move(setter));
}

void Condition::SetPropertyName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::Detail::ValueConversion::Trim(value))) return;
    propertyName_ = std::move(candidate);
}

void Condition::SetSourceName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(
            ::Aero::Base::Detail::ValueConversion::Trim(value))) return;
    sourceName_ = std::move(candidate);
}

Base::Result<void> MultiTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    return condition ? conditions_.PushBack(std::move(condition))
        : Base::Result<void>(InvalidStyle("MultiTrigger condition is null"));
}

Base::Result<void> MultiTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    return setter ? authoredSetters_.PushBack(std::move(setter))
        : Base::Result<void>(InvalidStyle("MultiTrigger setter is null"));
}

Base::Result<void> MultiDataTrigger::AddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) {
        return InvalidStyle(
            "MultiDataTrigger condition is null");
    }
    return conditions_.PushBack(
        std::move(condition));
}

Base::Result<void>
MultiDataTrigger::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "MultiDataTrigger authored setter is null");
    }
    return authoredSetters_.PushBack(
        std::move(setter));
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
        sizeof(Access), alignof(Access), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Access), alignof(Access), Base::MemoryTag::Ui);
    }
    program_ = new (memory) Access{};
}

Style::~Style() {
    if (program_ == nullptr) return;
    program_->~Access();
    implAllocator_->Deallocate(
        program_, sizeof(Access), alignof(Access), Base::MemoryTag::Ui);
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
    Base::Ref<Setter> setter) noexcept {
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
        trigger.setterProperties_.Empty() ||
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
        for (const Base::Ref<Setter>& authored : authoredSetterObjects_) {
            if (!authored) continue;
            Base::StringView name = authored->GetPropertyName();
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
            Access::RuntimeSetters(*basedOn_));
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
            nextTriggers.Append(Access::RuntimeTriggers(*basedOn_));
        if (!inherited) return inherited.GetStatus();
    }
    for (const TriggerPlan& trigger : program_->authoredTriggers) {
        if (trigger.IsBindingTrigger()) {
            if (!trigger.binding || trigger.value.IsUnset()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger Binding or Value is incomplete");
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
    basedOn_ = nullptr;
    basedOnOwner_.Reset();
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

Base::Result<void> Style::Access::Seal(
    Style& style,
    const void* properties) noexcept {
    return style.SealRuntime(properties);
}

Base::Span<const StyleSetter> Style::Access::RuntimeSetters(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Setters()
        : Base::Span<const StyleSetter>{};
}

Base::Span<const TriggerPlan> Style::Access::RuntimeTriggers(
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
        EnsureTriggerPhaseHook(object);
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
        RemovePendingTriggerEvaluation(object);
        UnsubscribeTriggers(
            object, *applications_[existing].style);
        Base::Result<void> triggers = ClearTriggerSetters(
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
    if (existing == UINT32_MAX) {
        Application application;
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
        Base::Result<void> subscribed =
            SubscribeTriggers(object, style);
        if (!subscribed) return subscribed.GetStatus();
    }
    return EvaluateTriggers(object, style);
}

Base::Result<void> StyleEngine::SetBindingTriggerState(
    DependencyObject& object,
    const Style& style,
    std::uint32_t triggerIndex,
    bool active) noexcept {
    const std::uint32_t applicationIndex = FindApplication(object);
    if (applicationIndex == UINT32_MAX ||
        applications_[applicationIndex].style != &style ||
        triggerIndex >= StylePrivate::RuntimeTriggers(style).Size() ||
        !StylePrivate::RuntimeTriggers(style)[triggerIndex].IsBindingTrigger()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style DataTrigger application was not found");
    }
    Application& application = applications_[applicationIndex];
    application.bindingTriggerKnown[triggerIndex] = 1U;
    application.bindingTriggerStates[triggerIndex] = active ? 1U : 0U;
    return EvaluateTriggers(object, style);
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
    UnsubscribeTriggers(object, *actual);
    Base::Result<void> triggers =
        ClearTriggerSetters(object, *actual);
    if (!triggers) return triggers.GetStatus();
    Base::Result<void> cleared = ClearSetters(object, *actual);
    if (!cleared) {
        return cleared.GetStatus();
    }
    if (existing != UINT32_MAX) {
        RemovePendingTriggerEvaluation(object);
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
    RemovePendingTriggerEvaluation(object);
    UnsubscribeTriggers(object, *applications_[existing].style);
    Base::Result<void> triggers =
        ClearTriggerSetters(object, *applications_[existing].style);
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
    for (const StyleSetter& setter : StylePrivate::RuntimeSetters(style)) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> cleared = values_->ClearStyleValue(object, setter.property);
        if (!cleared) {
            return cleared.GetStatus();
        }
    }
    return {};
}

Base::Result<void> StyleEngine::SubscribeTriggers(
    DependencyObject& object,
    const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < StylePrivate::RuntimeTriggers(style).Size();
         ++index) {
        const TriggerPlan& trigger =
            StylePrivate::RuntimeTriggers(style)[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                (StylePrivate::RuntimeTriggers(style)[previous].IsBindingTrigger() ||
                 StylePrivate::RuntimeTriggers(style)[previous].property != property);
        }
        if (!first) continue;
        Base::Result<void> subscribed =
            object.AddValueChangedHandlerChecked(
                property, propertyChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    return {};
}

void StyleEngine::UnsubscribeTriggers(
    DependencyObject& object,
    const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < StylePrivate::RuntimeTriggers(style).Size();
         ++index) {
        const TriggerPlan& trigger =
            StylePrivate::RuntimeTriggers(style)[index];
        if (trigger.IsBindingTrigger()) continue;
        const DependencyPropertyHandle property = trigger.property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                (StylePrivate::RuntimeTriggers(style)[previous].IsBindingTrigger() ||
                 StylePrivate::RuntimeTriggers(style)[previous].property != property);
        }
        if (first) {
            (void)object.RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
}

Base::Result<void> StyleEngine::EvaluateTriggers(
    DependencyObject& object,
    const Style& style) noexcept {
    const std::uint32_t applicationIndex =
        FindApplication(object);
    if (applicationIndex == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style application was not found while evaluating triggers");
    }
    Application& application =
        applications_[applicationIndex];
    Base::Result<void> cleared =
        ClearTriggerSetters(object, style);
    if (!cleared) return cleared.GetStatus();
    const Base::Span<const TriggerPlan> triggers =
        StylePrivate::RuntimeTriggers(style);
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        bool active = false;
        if (trigger.IsBindingTrigger()) {
            active = application.bindingTriggerKnown[index] != 0U &&
                application.bindingTriggerStates[index] != 0U;
        } else {
            Base::Result<PropertyValue> current =
                object.GetValue(trigger.property);
            if (!current) return current.GetStatus();
            active = current.Value() == trigger.value;
        }
        if (active) {
            for (const StyleTriggerSetter& setter :
                 trigger.setters) {
                if (IsDeferredBindingSetterValue(setter.value)) {
                    continue;
                }
                Base::Result<void> applied =
                    values_->SetTriggerValue(
                        object, setter.property,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        }
    }
    Base::Result<std::uint32_t> flushed =
        values_->Flush();
    if (!flushed) return flushed.GetStatus();
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const TriggerPlan& trigger =
            triggers[index];
        bool active = false;
        if (trigger.IsBindingTrigger()) {
            active = application.bindingTriggerKnown[index] != 0U &&
                application.bindingTriggerStates[index] != 0U;
        } else {
            Base::Result<PropertyValue> current =
                object.GetValue(trigger.property);
            if (!current) return current.GetStatus();
            active = current.Value() == trigger.value;
        }
        const bool wasActive =
            application.triggerStates[index] != 0U;
        if (active == wasActive) continue;
        application.triggerStates[index] =
            active ? 1U : 0U;
        Base::Result<void> actions =
            ExecuteTriggerActions(
                object,
                active
                    ? trigger.enterActions.AsSpan()
                    : trigger.exitActions.AsSpan());
        if (!actions) {
            lastActionStatus_ = actions.GetStatus();
            return actions.GetStatus();
        }
    }
    return {};
}

Base::Result<void> StyleEngine::ExecuteTriggerActions(
    DependencyObject& object,
    Base::Span<const Base::Ref<Base::Object>>
        actions) noexcept {
    if (actions.Empty() ||
        triggerActionHandler_ == nullptr) {
        return {};
    }
    return triggerActionHandler_(
        object, actions, triggerActionContext_);
}

Base::Result<void> StyleEngine::ClearTriggerSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    const Base::Span<const TriggerPlan> triggers =
        StylePrivate::RuntimeTriggers(style);
    for (std::uint32_t triggerIndex = 0U;
         triggerIndex < triggers.Size();
         ++triggerIndex) {
        const TriggerPlan& trigger =
            triggers[triggerIndex];
        for (std::uint32_t setterIndex = 0U;
             setterIndex < trigger.setters.Size();
             ++setterIndex) {
            if (IsDeferredBindingSetterValue(
                    trigger.setters[setterIndex].value)) {
                continue;
            }
            const DependencyPropertyHandle property =
                trigger.setters[setterIndex].property;
            bool first = true;
            for (std::uint32_t earlierTrigger = 0U;
                 earlierTrigger <= triggerIndex;
                 ++earlierTrigger) {
                const TriggerPlan& earlier =
                    triggers[earlierTrigger];
                const std::uint32_t limit =
                    earlierTrigger == triggerIndex
                        ? setterIndex : earlier.setters.Size();
                for (std::uint32_t earlierSetter = 0U;
                     earlierSetter < limit;
                     ++earlierSetter) {
                    first = first &&
                        earlier.setters[earlierSetter].property !=
                            property;
                }
            }
            if (!first) continue;
            Base::Result<void> cleared =
                values_->ClearTriggerValue(object, property);
            if (!cleared) return cleared.GetStatus();
        }
    }
    return {};
}

void StyleEngine::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const std::uint32_t index = FindApplication(object);
    if (index == UINT32_MAX) return;
    const Style& style = *applications_[index].style;
    for (const TriggerPlan& trigger : StylePrivate::RuntimeTriggers(style)) {
        if (!trigger.IsBindingTrigger() &&
            trigger.property == args.GetProperty()) {
            if (values_->IsFlushing()) {
                Base::Result<void> queued =
                    QueueTriggerEvaluation(object);
                if (!queued) {
                    lastActionStatus_ =
                        queued.GetStatus();
                }
            } else {
                Base::Result<void> evaluated =
                    EvaluateTriggers(object, style);
                if (!evaluated) {
                    lastActionStatus_ =
                        evaluated.GetStatus();
                }
            }
            return;
        }
    }
}

StyleEngine::~StyleEngine() noexcept {
    if (dispatcher_ != nullptr &&
        triggerPhaseHook_.IsValid() &&
        dispatcher_->CheckAccess()) {
        static_cast<void>(
            dispatcher_->RemoveFrameHook(
                triggerPhaseHook_));
    }
}

Base::Result<void> StyleEngine::EnsureTriggerPhaseHook(
    DependencyObject& object) noexcept {
    Dispatcher& dispatcher = object.GetDispatcher();
    if (triggerPhaseHook_.IsValid()) {
        return dispatcher_ == &dispatcher
            ? Base::Result<void>()
            : Base::Result<void>(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StyleEngine objects must share one Dispatcher"));
    }
    Base::Result<DispatcherFrameHookHandle> hook =
        dispatcher.RegisterFrameHook(
            DispatcherFramePhase::DataBind,
            &StyleEngine::TriggerPhaseHook,
            this,
            nullptr);
    if (!hook) return hook.GetStatus();
    dispatcher_ = &dispatcher;
    triggerPhaseHook_ = hook.Value();
    return {};
}

Base::Result<void> StyleEngine::QueueTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (DependencyObject* pending :
         pendingTriggerEvaluations_) {
        if (pending == &object) return {};
    }
    return pendingTriggerEvaluations_.PushBack(
        &object);
}

void StyleEngine::RemovePendingTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (std::uint32_t index = 0U;
         index < pendingTriggerEvaluations_.Size();) {
        if (pendingTriggerEvaluations_[index] != &object) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < pendingTriggerEvaluations_.Size();
             ++next) {
            pendingTriggerEvaluations_[next - 1U] =
                pendingTriggerEvaluations_[next];
        }
        pendingTriggerEvaluations_.PopBack();
    }
}

Base::Result<std::uint32_t>
StyleEngine::FlushPendingTriggerEvaluations() noexcept {
    Base::Vector<DependencyObject*> pending =
        std::move(pendingTriggerEvaluations_);
    std::uint32_t evaluatedCount = 0U;
    for (DependencyObject* object : pending) {
        if (object == nullptr) continue;
        const std::uint32_t index =
            FindApplication(*object);
        if (index == UINT32_MAX) continue;
        Base::Result<void> evaluated =
            EvaluateTriggers(
                *object,
                *applications_[index].style);
        if (!evaluated) return evaluated.GetStatus();
        ++evaluatedCount;
    }
    return evaluatedCount;
}

void StyleEngine::TriggerPhaseHook(
    void* context) noexcept {
    auto* manager =
        static_cast<StyleEngine*>(context);
    if (manager == nullptr ||
        !manager->lastActionStatus_.IsOk()) {
        return;
    }
    Base::Result<std::uint32_t> flushed =
        manager->FlushPendingTriggerEvaluations();
    if (!flushed) {
        manager->lastActionStatus_ =
            flushed.GetStatus();
    }
}


} // namespace Aero

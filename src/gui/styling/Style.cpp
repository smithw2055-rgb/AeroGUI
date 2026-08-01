#include "gui/styling/ThemeStyleRegistry.hpp"
#include "gui/styling/StyleAccess.hpp"
#include <Aero/Styling.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Meta/ValueConversion.hpp>

#include "gui/resources/ResourceAssignment.hpp"
#include "gui/styling/StyleRuntime.hpp"

namespace Aero {

std::uint32_t SetterBaseCollection::Count() const noexcept {
    return owner_ != nullptr ? owner_->GetAuthoredSetters().Size() : 0U;
}

SetterBase* SetterBaseCollection::At(std::uint32_t index) const noexcept {
    if (owner_ == nullptr || index >= owner_->GetAuthoredSetters().Size()) return nullptr;
    return owner_->GetAuthoredSetters()[index].Get();
}

void SetterBaseCollection::Add(Base::Ref<Setter> setter) noexcept {
    if (owner_ == nullptr) return;
    Base::Result<void> added = owner_->TryAddAuthoredSetter(std::move(setter));
    if (!added) Base::ReportOutOfMemory(sizeof(Setter), alignof(Setter), Base::MemoryTag::Ui);
}

void SetterBaseCollection::Clear() noexcept {
    if (owner_ != nullptr) static_cast<void>(owner_->ClearAuthoredSetters());
}

std::uint32_t TriggerCollection::Count() const noexcept {
    return owner_ != nullptr ? owner_->GetAuthoredTriggers().Size() : 0U;
}

TriggerBase* TriggerCollection::At(std::uint32_t index) const noexcept {
    if (owner_ == nullptr || index >= owner_->GetAuthoredTriggers().Size()) return nullptr;
    return owner_->GetAuthoredTriggers()[index].Get();
}

void TriggerCollection::Add(Base::Ref<PropertyTrigger> trigger) noexcept {
    if (owner_ == nullptr) return;
    Base::Result<void> added = owner_->TryAddAuthoredTrigger(std::move(trigger));
    if (!added) Base::ReportOutOfMemory(sizeof(PropertyTrigger), alignof(PropertyTrigger), Base::MemoryTag::Ui);
}

void TriggerCollection::Clear() noexcept {
    if (owner_ != nullptr) static_cast<void>(owner_->ClearAuthoredTriggers());
}


using namespace Aero::Core;
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
    const DependencyProperty& property,
    const PropertyValue& value) noexcept {
    if (property.Name() != Base::StringView("FontFamily") ||
        property.ValueType() != Core::TypeOf<Base::String>() ||
        value.Kind() != ValueKind::Object || value.IsNullObject() ||
        !value.AsObject() ||
        value.Type() != Media::FontFamily::StaticTypeId()) {
        return value;
    }
    return PropertyValue::TryFromString(
        property.ValueType(),
        static_cast<Media::FontFamily*>(value.AsObject().Get())->Source());
}

} // namespace


Base::Result<void> Style::Impl::Freeze(
    TypeId valueTargetType,
    Base::Vector<StyleSetter>&& valueSetters,
    Base::Vector<StylePropertyTrigger>&& valueTriggers) noexcept {
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

void Style::Impl::Reset() noexcept {
    targetType = InvalidTypeId;
    setters.Clear();
    triggers.Clear();
    frozen = false;
}

Base::Result<void> Setter::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return InvalidStyle(
            "Setter property name is empty");
    }
    return propertyName_.TryAssign(value);
}

Base::Result<void> Setter::SetTargetName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return InvalidStyle(
            "Setter target name is empty");
    }
    return targetName_.TryAssign(value);
}

Base::Result<void> Setter::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return InvalidStyle(
            "Setter authored value is unset");
    }
    authoredValue_ = value;
    return {};
}

Base::Result<void> Setter::Resolve(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> configured =
        SetProperty(property);
    if (!configured) return configured.GetStatus();
    return SetValue(value);
}

Base::Result<void> TriggerBase::TryAddEnterAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger enter action is null");
    }
    return enterActions_.TryPushBack(
        std::move(action));
}

Base::Result<void> TriggerBase::TryAddExitAction(
    Base::Ref<Base::Object> action) noexcept {
    if (!action) {
        return InvalidStyle(
            "Trigger exit action is null");
    }
    return exitActions_.TryPushBack(
        std::move(action));
}

Base::Result<void> PropertyTrigger::SetProperty(
    DependencyPropertyHandle value) noexcept {
    if (!value.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PropertyTrigger property is invalid");
    }
    property_ = value;
    return {};
}

Base::Result<void> PropertyTrigger::SetValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PropertyTrigger value is unset");
    }
    value_ = value;
    return {};
}

Base::Result<void> PropertyTrigger::TryAddSetter(
    const Setter& setter) noexcept {
    if (!setter.GetProperty().IsValid() ||
        setter.GetValue().IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PropertyTrigger setter is invalid");
    }
    return setters_.TryPushBack({
        setter.GetProperty(), setter.GetValue()});
}

Base::Result<void> PropertyTrigger::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return InvalidStyle(
            "Trigger property name is empty");
    }
    return propertyName_.TryAssign(value);
}

Base::Result<void> PropertyTrigger::SetSourceName(
    Base::StringView value) noexcept {
    return sourceName_.TryAssign(
        Core::ValueConversion::Trim(value));
}

Base::Result<void> PropertyTrigger::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return InvalidStyle(
            "Trigger authored value is unset");
    }
    authoredValue_ = value;
    return {};
}

Base::Result<void> PropertyTrigger::TryAddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "Trigger authored setter is null");
    }
    return authoredSetters_.TryPushBack(
        std::move(setter));
}

Base::Result<void>
PropertyTrigger::ClearAuthoredSetters() noexcept {
    authoredSetters_.Clear();
    return {};
}

Base::Result<StylePropertyTrigger>
PropertyTrigger::BuildPlan() const noexcept {
    if (!property_.IsValid() || value_.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "PropertyTrigger is incomplete");
    }
    StylePropertyTrigger plan;
    plan.property = property_;
    plan.value = value_;
    Base::Result<void> copied =
        plan.setters.TryAppend(setters_.AsSpan());
    if (!copied) {
        return copied.GetStatus();
    }
    copied = plan.enterActions.TryAppend(
        GetEnterActions());
    if (!copied) return copied.GetStatus();
    copied = plan.exitActions.TryAppend(
        GetExitActions());
    if (!copied) return copied.GetStatus();
    return plan;
}

Base::Result<void> DataTrigger::TryAddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "DataTrigger authored setter is null");
    }
    return authoredSetters_.TryPushBack(
        std::move(setter));
}

Base::Result<void> Condition::SetPropertyName(
    Base::StringView value) noexcept {
    return propertyName_.TryAssign(Core::ValueConversion::Trim(value));
}

Base::Result<void> Condition::SetSourceName(
    Base::StringView value) noexcept {
    return sourceName_.TryAssign(Core::ValueConversion::Trim(value));
}

Base::Result<void> MultiTrigger::TryAddCondition(
    Base::Ref<Condition> condition) noexcept {
    return condition ? conditions_.TryPushBack(std::move(condition))
        : Base::Result<void>(InvalidStyle("MultiTrigger condition is null"));
}

Base::Result<void> MultiTrigger::TryAddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    return setter ? authoredSetters_.TryPushBack(std::move(setter))
        : Base::Result<void>(InvalidStyle("MultiTrigger setter is null"));
}

Base::Result<void> MultiDataTrigger::TryAddCondition(
    Base::Ref<Condition> condition) noexcept {
    if (!condition) {
        return InvalidStyle(
            "MultiDataTrigger condition is null");
    }
    return conditions_.TryPushBack(
        std::move(condition));
}

Base::Result<void>
MultiDataTrigger::TryAddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (!setter) {
        return InvalidStyle(
            "MultiDataTrigger authored setter is null");
    }
    return authoredSetters_.TryPushBack(
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
      authored_(),
      authoredTriggers_(),
      program_(),
      resources_() {}

Base::Result<void> Style::TrySetTargetType(TypeId targetType) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (targetType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style target type is invalid");
    }
    targetType_ = targetType;
    return {};
}

Base::Result<void> Style::TrySetBasedOn(const Style* basedOn) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (basedOn == this) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Style cannot be BasedOn itself");
    }
    basedOn_ = basedOn;
    return {};
}

Base::Result<void> Style::TryAddSetter(
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
    for (const StyleSetter& setter : authored_) {
        if (setter.property == property) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Style already has a setter for this property");
        }
    }
    return authored_.TryPushBack({property, value});
}

Base::Result<void> Style::TrySetBasedOn(
    Base::Ref<Base::Object> basedOn) noexcept {
    if (basedOn &&
        basedOn->RuntimeType() != RuntimeType()) {
        return InvalidStyle(
            "BasedOn object is not a compatible Style");
    }
    Base::Result<void> assigned =
        TrySetBasedOn(
            static_cast<Style*>(basedOn.Get()));
    if (!assigned) return assigned.GetStatus();
    basedOnOwner_ = std::move(basedOn);
    return {};
}

Base::Result<void> Style::TryAddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    if (!setter) {
        return InvalidStyle(
            "Style authored setter is null");
    }
    return authoredSetterObjects_.TryPushBack(
        std::move(setter));
}

Base::Result<void> Style::TryAddAuthoredTrigger(
    Base::Ref<PropertyTrigger> trigger) noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    if (!trigger) {
        return InvalidStyle(
            "Style authored trigger is null");
    }
    return authoredTriggerObjects_.TryPushBack(
        std::move(trigger));
}

Base::Result<void> Style::ClearAuthoredSetters() noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    authoredSetterObjects_.Clear();
    return {};
}

Base::Result<void> Style::ClearAuthoredTriggers() noexcept {
    if (sealed_) {
        return InvalidStyle(
            "Cannot modify a sealed Style");
    }
    authoredTriggerObjects_.Clear();
    return {};
}

Base::Result<void> Style::TryAddSetter(
    const Setter& setter) noexcept {
    return TryAddSetter(
        setter.GetProperty(), setter.GetValue());
}

Base::Result<void> Style::TryAddPropertyTrigger(
    StylePropertyTrigger trigger) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!trigger.property.IsValid() || trigger.value.IsUnset() ||
        trigger.setters.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style property trigger is incomplete");
    }
    return authoredTriggers_.TryPushBack(std::move(trigger));
}

Base::Result<void> Style::TryAddPropertyTrigger(
    const PropertyTrigger& trigger) noexcept {
    Base::Result<StylePropertyTrigger> plan =
        trigger.BuildPlan();
    if (!plan) {
        return plan.GetStatus();
    }
    return TryAddPropertyTrigger(
        std::move(plan).Value());
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
        Base::Result<void> inherited = next.TryAppend(basedOn_->Setters());
        if (!inherited) {
            return inherited.GetStatus();
        }
    }
    for (const StyleSetter& setter : authored_) {
        const DependencyProperty* property = properties.Find(setter.property);
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
            Base::Result<void> appended = next.TryPushBack({
                setter.property, normalizedValue});
            if (!appended) {
                return appended.GetStatus();
            }
        }
    }
    Base::Vector<StylePropertyTrigger> nextTriggers;
    if (basedOn_ != nullptr) {
        Base::Result<void> inherited =
            nextTriggers.TryAppend(basedOn_->Triggers());
        if (!inherited) return inherited.GetStatus();
    }
    for (const StylePropertyTrigger& trigger : authoredTriggers_) {
        const DependencyProperty* condition =
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
        for (std::uint32_t index = 0U;
             index < trigger.setters.Size();
             ++index) {
            const StyleTriggerSetter& setter = trigger.setters[index];
            const DependencyProperty* property =
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
            nextTriggers.TryPushBack(trigger);
        if (!appended) return appended.GetStatus();
    }
    Base::Result<void> frozenProgram = program_.Freeze(
        targetType_, std::move(next), std::move(nextTriggers));
    if (!frozenProgram) return frozenProgram.GetStatus();
    Base::Result<void> sealedResources = resources_.Seal();
    if (!sealedResources) {
        program_.Reset();
        return sealedResources.GetStatus();
    }
    authored_.Clear();
    authoredTriggers_.Clear();
    authoredSetterObjects_.Clear();
    authoredTriggerObjects_.Clear();
    basedOn_ = nullptr;
    basedOnOwner_.Reset();
    sealed_ = true;
    return {};
}

Base::Result<void> Aero::Detail::ThemeStyleRegistry::TryRegister(
    TypeId controlType,
    const Style& style) noexcept {
    if (properties_ == nullptr || !properties_->IsFrozen() ||
        controlType == InvalidTypeId ||
        properties_->Types().FindType(controlType) == nullptr ||
        !style.IsSealed() ||
        style.GetTargetType() != controlType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Theme style registration requires a sealed exact-type Style");
    }
    for (const Entry& entry : entries_) {
        if (entry.controlType == controlType) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "A theme style is already registered for this control type");
        }
    }
    return entries_.TryPushBack({controlType, &style});
}

const Aero::Style* Aero::Detail::ThemeStyleRegistry::Find(
    TypeId controlType) const noexcept {
    if (properties_ == nullptr) return nullptr;
    TypeId current = controlType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId &&
         depth <= properties_->Types().TypeCount();
         ++depth) {
        for (const Entry& entry : entries_) {
            if (entry.controlType == current) return entry.style;
        }
        const TypeInfo* type =
            properties_->Types().FindType(current);
        if (type == nullptr) break;
        current = type->BaseType();
    }
    return nullptr;
}

Base::Result<void> Style::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Aero::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "Style Resources is already assigned");
}

} // namespace Aero

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero;

Base::Result<void> StyleManager::VerifyTarget(
    const DependencyObject& object,
    const Style& style) const noexcept {
    if (values_ == nullptr || properties_ == nullptr || !style.IsSealed()) {
        return InvalidStyle("StyleManager requires a sealed Style");
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

Base::Result<void> StyleManager::Apply(
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
    for (const StyleSetter& setter : style.Setters()) {
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
            application.triggerStates.TryResize(
                style.Triggers().Size(), 0U);
        if (!states) return states.GetStatus();
        Base::Result<void> tracked =
            applications_.TryPushBack(
                std::move(application));
        if (!tracked) {
            return tracked.GetStatus();
        }
    } else if (requiresSubscription) {
        applications_[existing].style = &style;
        Base::Result<void> states =
            applications_[existing].
                triggerStates.TryResize(
                    style.Triggers().Size(), 0U);
        if (!states) return states.GetStatus();
    }
    if (requiresSubscription) {
        Base::Result<void> subscribed =
            SubscribeTriggers(object, style);
        if (!subscribed) return subscribed.GetStatus();
    }
    return EvaluateTriggers(object, style);
}

Base::Result<void> StyleManager::Clear(
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

Base::Result<bool> StyleManager::DetachObject(
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

const Style* StyleManager::AppliedStyle(
    const DependencyObject& object)
    const noexcept {
    const std::uint32_t application =
        FindApplication(object);
    return application != UINT32_MAX
        ? applications_[application].style
        : nullptr;
}

std::uint32_t StyleManager::FindApplication(
    const DependencyObject& object) const noexcept {
    for (std::uint32_t index = 0U; index < applications_.Size(); ++index) {
        if (applications_[index].object == &object) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<void> StyleManager::ClearSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    for (const StyleSetter& setter : style.Setters()) {
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

Base::Result<void> StyleManager::SubscribeTriggers(
    DependencyObject& object,
    const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < style.Triggers().Size();
         ++index) {
        const DependencyPropertyHandle property =
            style.Triggers()[index].property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                style.Triggers()[previous].property != property;
        }
        if (!first) continue;
        Base::Result<void> subscribed =
            object.TryAddValueChangedHandler(
                property, propertyChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    return {};
}

void StyleManager::UnsubscribeTriggers(
    DependencyObject& object,
    const Style& style) noexcept {
    for (std::uint32_t index = 0U;
         index < style.Triggers().Size();
         ++index) {
        const DependencyPropertyHandle property =
            style.Triggers()[index].property;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                style.Triggers()[previous].property != property;
        }
        if (first) {
            (void)object.RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
}

Base::Result<void> StyleManager::EvaluateTriggers(
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
    const Base::Span<const StylePropertyTrigger> triggers =
        style.Triggers();
    for (std::uint32_t index = 0U;
         index < triggers.Size(); ++index) {
        const StylePropertyTrigger& trigger =
            triggers[index];
        Base::Result<PropertyValue> current =
            object.GetValue(trigger.property);
        if (!current) return current.GetStatus();
        const bool active =
            current.Value() == trigger.value;
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
        const StylePropertyTrigger& trigger =
            triggers[index];
        Base::Result<PropertyValue> current =
            object.GetValue(trigger.property);
        if (!current) return current.GetStatus();
        const bool active =
            current.Value() == trigger.value;
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

Base::Result<void> StyleManager::ExecuteTriggerActions(
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

Base::Result<void> StyleManager::ClearTriggerSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    const Base::Span<const StylePropertyTrigger> triggers =
        style.Triggers();
    for (std::uint32_t triggerIndex = 0U;
         triggerIndex < triggers.Size();
         ++triggerIndex) {
        const StylePropertyTrigger& trigger =
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
                const StylePropertyTrigger& earlier =
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

void StyleManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const std::uint32_t index = FindApplication(object);
    if (index == UINT32_MAX) return;
    const Style& style = *applications_[index].style;
    for (const StylePropertyTrigger& trigger : style.Triggers()) {
        if (trigger.property == args.property) {
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

StyleManager::~StyleManager() noexcept {
    if (dispatcher_ != nullptr &&
        triggerPhaseHook_.IsValid() &&
        dispatcher_->CheckAccess()) {
        static_cast<void>(
            dispatcher_->RemoveFrameHook(
                triggerPhaseHook_));
    }
}

Base::Result<void> StyleManager::EnsureTriggerPhaseHook(
    DependencyObject& object) noexcept {
    Dispatcher& dispatcher = object.GetDispatcher();
    if (triggerPhaseHook_.IsValid()) {
        return dispatcher_ == &dispatcher
            ? Base::Result<void>()
            : Base::Result<void>(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StyleManager objects must share one Dispatcher"));
    }
    Base::Result<DispatcherFrameHookHandle> hook =
        dispatcher.RegisterFrameHook(
            DispatcherFramePhase::DataBind,
            &StyleManager::TriggerPhaseHook,
            this,
            nullptr);
    if (!hook) return hook.GetStatus();
    dispatcher_ = &dispatcher;
    triggerPhaseHook_ = hook.Value();
    return {};
}

Base::Result<void> StyleManager::QueueTriggerEvaluation(
    DependencyObject& object) noexcept {
    for (DependencyObject* pending :
         pendingTriggerEvaluations_) {
        if (pending == &object) return {};
    }
    return pendingTriggerEvaluations_.TryPushBack(
        &object);
}

void StyleManager::RemovePendingTriggerEvaluation(
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
StyleManager::FlushPendingTriggerEvaluations() noexcept {
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

void StyleManager::TriggerPhaseHook(
    void* context) noexcept {
    auto* manager =
        static_cast<StyleManager*>(context);
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

Base::Result<bool> ThemeStyleManager::ApplyDefault(
    DependencyObject& object) noexcept {
    if (values_ == nullptr || registry_ == nullptr) {
        return InvalidStyle(
            "ThemeStyleManager is not configured").GetStatus();
    }
    const Style* style = registry_->Find(object.RuntimeType());
    if (style == nullptr) return false;
    const std::uint32_t existing = FindApplication(object);
    if (existing != UINT32_MAX &&
        applications_[existing].style != style) {
        Base::Result<void> cleared = ClearSetters(
            object, *applications_[existing].style);
        if (!cleared) return cleared.GetStatus();
    }
    for (const StyleSetter& setter : style->Setters()) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> applied =
            values_->SetThemeStyleValue(
                object, setter.property, setter.value);
        if (!applied) return applied.GetStatus();
    }
    if (existing == UINT32_MAX) {
        Base::Result<void> tracked =
            applications_.TryPushBack({&object, style});
        if (!tracked) return tracked.GetStatus();
    } else {
        applications_[existing].style = style;
    }
    return true;
}

Base::Result<bool> ThemeStyleManager::Clear(
    DependencyObject& object) noexcept {
    const std::uint32_t existing = FindApplication(object);
    if (existing == UINT32_MAX) return false;
    Base::Result<void> cleared =
        ClearSetters(object, *applications_[existing].style);
    if (!cleared) return cleared.GetStatus();
    if (existing + 1U != applications_.Size()) {
        applications_[existing] =
            applications_[applications_.Size() - 1U];
    }
    applications_.PopBack();
    return true;
}

std::uint32_t ThemeStyleManager::FindApplication(
    const DependencyObject& object) const noexcept {
    for (std::uint32_t index = 0U;
         index < applications_.Size();
         ++index) {
        if (applications_[index].object == &object) return index;
    }
    return UINT32_MAX;
}

Base::Result<void> ThemeStyleManager::ClearSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    for (const StyleSetter& setter : style.Setters()) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> cleared =
            values_->ClearThemeStyleValue(
                object, setter.property);
        if (!cleared) return cleared.GetStatus();
    }
    return {};
}

} // namespace Aero::Detail

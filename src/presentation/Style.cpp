#include <Aero/Presentation/Style.hpp>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsTargetCompatible(
    const TypeRegistry& types,
    TypeId derived,
    TypeId expectedBase) noexcept {
    return derived == expectedBase || types.IsDerivedFrom(derived, expectedBase);
}

} // namespace


Base::Result<void> StyleProgram::Freeze(
    TypeId targetType,
    Base::Vector<StyleSetter>&& setters,
    Base::Vector<StylePropertyTrigger>&& triggers) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "StyleProgram is already frozen");
    }
    if (targetType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StyleProgram target type is invalid");
    }
    targetType_ = targetType;
    setters_ = std::move(setters);
    triggers_ = std::move(triggers);
    frozen_ = true;
    return {};
}

void StyleProgram::Reset() noexcept {
    targetType_ = InvalidTypeId;
    setters_.Clear();
    triggers_.Clear();
    frozen_ = false;
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
    if (!setter.Property().IsValid() ||
        setter.Value().IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PropertyTrigger setter is invalid");
    }
    return setters_.TryPushBack({
        setter.Property(), setter.Value()});
}

Base::Result<void> PropertyTrigger::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return InvalidStyle(
            "Trigger property name is empty");
    }
    return propertyName_.TryAssign(value);
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
    return plan;
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

Base::Result<void> Style::TryAddSetter(
    const Setter& setter) noexcept {
    return TryAddSetter(
        setter.Property(), setter.Value());
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

Base::Result<void> Style::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
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
                properties.Types(), targetType_, ancestor->targetType_)) {
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
        Base::Result<void> validValue = properties.ValidateValueFor(
            setter.property, targetType_, setter.value);
        if (!validValue) {
            return validValue.GetStatus();
        }
        bool replaced = false;
        for (StyleSetter& inherited : next) {
            if (inherited.property == setter.property) {
                inherited.value = setter.value;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            Base::Result<void> appended = next.TryPushBack(setter);
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
            Base::Result<void> validValue = properties.ValidateValueFor(
                setter.property, targetType_, setter.value);
            if (!validValue) return validValue.GetStatus();
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

Base::Result<void> StyleManager::VerifyTarget(
    const DependencyObject& object,
    const Style& style) const noexcept {
    if (values_ == nullptr || properties_ == nullptr || !style.IsSealed()) {
        return InvalidStyle("StyleManager requires a sealed Style");
    }
    if (!IsTargetCompatible(
            properties_->Types(), object.RuntimeType(), style.TargetType())) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style target type is incompatible with the object type");
    }
    return {};
}

Base::Result<void> StyleManager::Apply(
    DependencyObject& object,
    const Style& style) noexcept {
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    const std::uint32_t existing = FindApplication(object);
    const bool requiresSubscription =
        existing == UINT32_MAX ||
        applications_[existing].style != &style;
    if (existing != UINT32_MAX && applications_[existing].style != &style) {
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
        Base::Result<void> applied = values_->SetStyleValue(
            object, setter.property, setter.value);
        if (!applied) {
            return applied.GetStatus();
        }
    }
    if (existing == UINT32_MAX) {
        Base::Result<void> tracked = applications_.TryPushBack({&object, &style});
        if (!tracked) {
            return tracked.GetStatus();
        }
    } else {
        applications_[existing].style = &style;
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
    Base::Result<void> cleared =
        ClearTriggerSetters(object, style);
    if (!cleared) return cleared.GetStatus();
    for (const StylePropertyTrigger& trigger : style.Triggers()) {
        Base::Result<PropertyValue> current =
            object.GetValue(trigger.property);
        if (!current) return current.GetStatus();
        if (current.Value() != trigger.value) continue;
        for (const StyleTriggerSetter& setter : trigger.setters) {
            Base::Result<void> applied = values_->SetTriggerValue(
                object, setter.property, setter.value);
            if (!applied) return applied.GetStatus();
        }
    }
    return {};
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
            (void)EvaluateTriggers(object, style);
            return;
        }
    }
}

Base::Result<void> ThemeStyleRegistry::TryRegister(
    TypeId controlType,
    const Style& style) noexcept {
    if (properties_ == nullptr || !properties_->IsFrozen() ||
        controlType == InvalidTypeId ||
        properties_->Types().FindType(controlType) == nullptr ||
        !style.IsSealed() ||
        style.TargetType() != controlType) {
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

const Style* ThemeStyleRegistry::Find(
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
        Base::Result<void> cleared =
            values_->ClearThemeStyleValue(
                object, setter.property);
        if (!cleared) return cleared.GetStatus();
    }
    return {};
}

} // namespace Aero::Presentation

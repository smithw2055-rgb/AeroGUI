#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerDiagnostics.hpp"
#include "gui/triggers/TriggerEngine.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Style.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Triggers/Triggers.hpp>
#include <Aero/Value.hpp>
#include <Aero/UIElement.hpp>

#include <new>

namespace Aero {

void Element::OnBlendingModeChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (!AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
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
        AeroGuiInternal::PropertyRegistry(object).Find(args.GetProperty());
    if (source == nullptr) return;

    const Meta::PropertyInfo* targetInfo =
        AeroGuiInternal::PropertyRegistry(object).Types().FindProperty(
            object.RuntimeType(), source->Name(), false);
    if (targetInfo == nullptr ||
        targetInfo->Id() == source->Handle().value) {
        return;
    }
    const Meta::DependencyProperty* target =
        AeroGuiInternal::PropertyRegistry(object).Find(
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
        AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
            value.AsObject()->RuntimeType(), target->ValueType())) {
        value = Meta::Value::FromObject(
            target->ValueType(), value.AsObject());
    }
    object.SetValue(target->Handle(), value);
}

std::uint32_t SetterBaseCollection::GetCount() const noexcept {
    return owner_ != nullptr ? owner_->GetAuthoredSetters().Size() : 0U;
}

SetterBase* SetterBaseCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr || index >= owner_->GetAuthoredSetters().Size()) return nullptr;
    return owner_->GetAuthoredSetters()[index].Get();
}

void SetterBaseCollection::Add(
    Base::Ref<SetterBase> setter) noexcept {
    if (owner_ == nullptr) { AERO_ASSERT(false); return; }
    owner_->AddAuthoredSetter(std::move(setter));
}

void SetterBaseCollection::Add(
    Base::Ref<Setter> setter) noexcept {
    Add(Base::Ref<SetterBase>(std::move(setter)));
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

void TriggerCollection::Add(
    Base::Ref<TriggerBase> trigger) noexcept {
    if (owner_ == nullptr) { AERO_ASSERT(false); return; }
    owner_->AddAuthoredTrigger(std::move(trigger));
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



struct Style::Program {
    static Base::Result<void> Seal(
        Style& style,
        const Meta::DependencyPropertyRegistry& properties) noexcept;
    static Base::Span<const StyleSetter> RuntimeSetters(
        const Style& style) noexcept;
    static Base::Span<const TriggerPlan> RuntimeTriggers(
        const Style& style) noexcept;
    static Base::Result<void> ApplySetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;
    static Base::Result<void> ClearSetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;

    Program() noexcept
        : authoredSetters(&Base::GetDefaultAllocator()),
          authoredTriggers(&Base::GetDefaultAllocator()),
          setters(&Base::GetDefaultAllocator()),
          triggers(&Base::GetDefaultAllocator()) {}
    Program(Program&&) noexcept = default;
    Program& operator=(Program&&) noexcept = default;
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    TypeId TargetType() const noexcept { return targetType; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return {setters.Data(), setters.Size()};
    }
    Base::Span<const TriggerPlan> Triggers() const noexcept {
        return {triggers.Data(), triggers.Size()};
    }
    Base::Result<void> Freeze(
        TypeId valueTargetType,
        Base::Vector<StyleSetter>&& valueSetters,
        Base::Vector<TriggerPlan>&& valueTriggers) noexcept;
    Base::Result<void> AddAuthoredSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> AddAuthoredTrigger(
        TriggerPlan trigger) noexcept;
    void ClearAuthored() noexcept;
    void Reset() noexcept;

    TypeId targetType = InvalidTypeId;
    Base::Vector<StyleSetter> authoredSetters;
    Base::Vector<TriggerPlan> authoredTriggers;
    Base::Vector<StyleSetter> setters;
    Base::Vector<TriggerPlan> triggers;
    bool frozen = false;
};

Base::Result<void> Style::Program::Freeze(
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

void Style::Program::Reset() noexcept {
    targetType = InvalidTypeId;
    setters.Clear();
    triggers.Clear();
    frozen = false;
}

Base::Result<void> Style::Program::AddAuthoredSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    for (const StyleSetter& setter : authoredSetters) {
        if (setter.property == property) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Style already has a setter for this property");
        }
    }
    authoredSetters.PushBack({property, value});
    return {};
}

Base::Result<void> Style::Program::AddAuthoredTrigger(
    TriggerPlan trigger) noexcept {
    authoredTriggers.PushBack(std::move(trigger));
    return {};
}

void Style::Program::ClearAuthored() noexcept {
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
        sizeof(Style::Program), alignof(Style::Program), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Style::Program), alignof(Style::Program), Base::MemoryTag::Ui);
    }
    program_ = new (memory) Style::Program{};
}

Style::~Style() {
    if (program_ == nullptr) return;
    program_->~Program();
    implAllocator_->Deallocate(
        program_, sizeof(Style::Program), alignof(Style::Program), Base::MemoryTag::Ui);
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

void Style::AddSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!property.IsValid() || value.IsUnset()) { AERO_ASSERT(false); return; }
    program_->AddAuthoredSetter(property, value);
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

void Style::AddAuthoredSetter(
    Base::Ref<SetterBase> setter) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!setter) { AERO_ASSERT(false); return; }
    authoredSetterObjects_.PushBack(
        std::move(setter));
}

void Style::AddAuthoredSetter(
    Base::Ref<Setter> setter) noexcept {
    AddAuthoredSetter(Base::Ref<SetterBase>(std::move(setter)));
}

void Style::AddAuthoredTrigger(
    Base::Ref<TriggerBase> trigger) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!trigger) { AERO_ASSERT(false); return; }
    authoredTriggerObjects_.PushBack(
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

void Style::AddSetter(
    const Setter& setter) noexcept {
    AddSetter(
        setter.GetProperty(), setter.GetValue());
}

void Style::AddPropertyTrigger(
    DependencyPropertyHandle condition,
    const PropertyValue& conditionValue,
    DependencyPropertyHandle property,
    PropertyValue value) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!condition.IsValid() || conditionValue.IsUnset() ||
        !property.IsValid() || value.IsUnset()) { AERO_ASSERT(false); return; }
    TriggerPlan trigger;
    trigger.property = condition;
    trigger.value = conditionValue;
    trigger.setters.PushBack(
        {property, std::move(value)});
    Base::Result<void> planned =
        program_->AddAuthoredTrigger(std::move(trigger));
    if (!planned) { AERO_ASSERT(false); return; }
}

void Style::AddTrigger(
    const Trigger& trigger) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!trigger.property_.IsValid() || trigger.value_.IsUnset() ||
        trigger.setterProperties_.Size() != trigger.setterValues_.Size()) { AERO_ASSERT(false); return; }
    TriggerPlan plan;
    plan.property = trigger.property_;
    plan.value = trigger.value_;
    for (std::uint32_t index = 0U;
         index < trigger.setterProperties_.Size(); ++index) {
        plan.setters.PushBack({
            trigger.setterProperties_[index], trigger.setterValues_[index]});
    }
    plan.enterActions.Append(
        trigger.GetEnterActions());
    plan.exitActions.Append(trigger.GetExitActions());
    Base::Result<void> planned =
        program_->AddAuthoredTrigger(std::move(plan));
    if (!planned) { AERO_ASSERT(false); return; }
}

void Style::AddTrigger(
    const DataTrigger& trigger) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!trigger.GetBinding() || trigger.GetAuthoredValue().IsUnset() ||
        trigger.GetAuthoredSetters().Empty()) { AERO_ASSERT(false); return; }
    TriggerPlan plan;
    plan.binding = trigger.GetBinding();
    plan.value = trigger.GetAuthoredValue();
    for (const Base::Ref<Setter>& authored :
         trigger.GetAuthoredSetters()) {
        if (!authored || !authored->GetProperty().IsValid() ||
            authored->GetValue().IsUnset()) { AERO_ASSERT(false); return; }
        plan.setters.PushBack({
            authored->GetProperty(), authored->GetValue()});
    }
    plan.enterActions.Append(
        trigger.GetEnterActions());
    plan.exitActions.Append(trigger.GetExitActions());
    Base::Result<void> planned =
        program_->AddAuthoredTrigger(std::move(plan));
    if (!planned) { AERO_ASSERT(false); return; }
}

void Style::AddTrigger(
    const MultiDataTrigger& trigger) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (trigger.GetConditions().Empty() ||
        trigger.GetAuthoredSetters().Empty()) { AERO_ASSERT(false); return; }
    TriggerPlan plan;
    bool first = true;
    for (const Base::Ref<Condition>& condition :
         trigger.GetConditions()) {
        if (!condition || !condition->GetBinding() ||
            condition->GetAuthoredValue().IsUnset()) { AERO_ASSERT(false); return; }
        if (first) {
            plan.binding = condition->GetBinding();
            plan.value = condition->GetAuthoredValue();
            first = false;
            continue;
        }
        TriggerBindingCondition extra;
        extra.binding = condition->GetBinding();
        extra.value = condition->GetAuthoredValue();
        plan.extraBindings.PushBack(std::move(extra));
    }
    for (const Base::Ref<Setter>& authored :
         trigger.GetAuthoredSetters()) {
        if (!authored || !authored->GetProperty().IsValid() ||
            authored->GetValue().IsUnset()) { AERO_ASSERT(false); return; }
        plan.setters.PushBack({
            authored->GetProperty(), authored->GetValue()});
    }
    plan.enterActions.Append(
        trigger.GetEnterActions());
    plan.exitActions.Append(trigger.GetExitActions());
    Base::Result<void> planned =
        program_->AddAuthoredTrigger(std::move(plan));
    if (!planned) { AERO_ASSERT(false); return; }
}

Base::Result<void> Style::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
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
        next.Append(
            Style::Program::RuntimeSetters(*basedOn_));
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
            next.PushBack({
                setter.property, normalizedValue});
        }
    }
    Base::Vector<TriggerPlan> nextTriggers;
    if (basedOn_ != nullptr) {
        nextTriggers.Append(Style::Program::RuntimeTriggers(*basedOn_));
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
        nextTriggers.PushBack(trigger);
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
    OnSeal();
    return {};
}

void Style::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::AssignResourceDictionary(
        resources_,
        std::move(value),
        "Style Resources is already assigned");
}

Base::Result<void> Style::Program::Seal(
    Style& style,
    const DependencyPropertyRegistry& properties) noexcept {
    return style.Seal(properties);
}

Base::Span<const StyleSetter> Style::Program::RuntimeSetters(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Setters()
        : Base::Span<const StyleSetter>{};
}

Base::Span<const TriggerPlan> Style::Program::RuntimeTriggers(
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

Base::Result<void> Style::Program::ApplySetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    for (const StyleSetter& setter : RuntimeSetters(style)) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> applied = values.SetStyleValue(
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
            element->AddHandler(
                eventSetter->GetEvent(),
                eventSetter->GetHandler());
        }
    }
    return {};
}

Base::Result<void> Style::Program::ClearSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    for (const StyleSetter& setter : RuntimeSetters(style)) {
        if (IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        Base::Result<void> cleared = values.ClearStyleValue(
            object, setter.property);
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
        triggerEngine_->EnableDataBindPhase(object);
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
    Base::Result<void> setters =
        Style::Program::ApplySetters(style, object, *values_);
    if (!setters) {
        return setters.GetStatus();
    }
    if (existing == UINT32_MAX) {
        StyleApplication application;
        application.object = &object;
        application.style = &style;
        application.triggerStates.Resize(
                Style::Program::RuntimeTriggers(style).Size(), 0U);
        application.bindingTriggerStates.Resize(
            Style::Program::RuntimeTriggers(style).Size(), 0U);
        application.bindingTriggerKnown.Resize(
            Style::Program::RuntimeTriggers(style).Size(), 0U);
        const std::uint32_t newIndex = applications_.Size();
        applications_.PushBack(
                std::move(application));
        static_cast<void>(objectIndexMap_.Insert(&object, newIndex));
    } else if (requiresSubscription) {
        applications_[existing].style = &style;
        applications_[existing].triggerStates.Resize(
                Style::Program::RuntimeTriggers(style).Size(), 0U);
        applications_[existing].bindingTriggerStates.Resize(
            Style::Program::RuntimeTriggers(style).Size(), 0U);
        applications_[existing].bindingTriggerKnown.Resize(
            Style::Program::RuntimeTriggers(style).Size(), 0U);
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
        objectIndexMap_.Erase(&object);
        if (existing + 1U != applications_.Size()) {
            applications_[existing] = std::move(applications_[applications_.Size() - 1U]);
            static_cast<void>(objectIndexMap_.Set(applications_[existing].object, existing));
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
    objectIndexMap_.Erase(&object);
    if (existing + 1U != applications_.Size()) {
        applications_[existing] = std::move(applications_[applications_.Size() - 1U]);
        static_cast<void>(objectIndexMap_.Set(applications_[existing].object, existing));
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
    const std::uint32_t* found = objectIndexMap_.Find(&object);
    return found != nullptr ? *found : UINT32_MAX;
}

Base::Result<void> StyleEngine::ClearSetters(
    DependencyObject& object,
    const Style& style) noexcept {
    DetachSetterBindings(object);
    return Style::Program::ClearSetters(style, object, *values_);
}

Base::Result<void> StyleEngine::AttachSetterBindings(
    DependencyObject& object,
    const Style& style) noexcept {
    BindingEngine* bindings = AeroGuiInternal::BindingEngineOf(object);
    for (const StyleSetter& setter : Style::Program::RuntimeSetters(style)) {
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
        Base::Object* source = binding.GetSource().Get();
        if (source == nullptr && !binding.GetElementName().Empty()) {
            if (auto* framework = ::Aero::TryCast<FrameworkElement>(&object)) {
                source = framework->FindName(binding.GetElementName());
            }
        } else if (source == nullptr && binding.GetRelativeSource()) {
            const Data::RelativeSourceMode mode =
                binding.GetRelativeSource()->GetMode();
            if (mode == Data::RelativeSourceMode::Self) {
                source = &object;
            } else if (mode == Data::RelativeSourceMode::TemplatedParent) {
                if (auto* framework = ::Aero::TryCast<FrameworkElement>(&object)) {
                    source = framework->GetTemplatedParent();
                }
            } else if (mode == Data::RelativeSourceMode::FindAncestor) {
                Base::StringView ancestorName =
                    binding.GetRelativeSource()->GetAncestorType();
                for (std::uint32_t nameIndex = 0U;
                     nameIndex < ancestorName.SizeBytes(); ++nameIndex) {
                    if (ancestorName[nameIndex] == ':') {
                        ancestorName = ancestorName.Substr(
                            nameIndex + 1U,
                            ancestorName.SizeBytes() - nameIndex - 1U);
                        break;
                    }
                }
                const std::uint32_t requestedLevel =
                    binding.GetRelativeSource()->GetAncestorLevel();
                std::uint32_t matchedLevel = 0U;
                Media::Visual* current = ::Aero::TryCast<Media::Visual>(&object);
                if (current != nullptr) {
                    Media::Visual* parent = ::Aero::TryCast<Media::Visual>(
                        current->GetLogicalParent());
                    if (parent == nullptr) {
                        parent = current->GetVisualParent();
                    }
                    current = parent;
                }
                while (current != nullptr) {
                    const Meta::TypeInfo* type =
                        bindings->Metadata()->Types().FindType(
                            current->RuntimeType());
                    const bool matchesType = ancestorName.Empty() ||
                        (type != nullptr && type->Name() == ancestorName);
                    if (matchesType && ++matchedLevel == requestedLevel) {
                        source = current;
                        break;
                    }
                    Media::Visual* next = ::Aero::TryCast<Media::Visual>(
                        current->GetLogicalParent());
                    if (next == nullptr) {
                        next = current->GetVisualParent();
                    }
                    current = next;
                }
            }
        }
        const bool isExplicitSource = binding.GetSource() ||
            !binding.GetElementName().Empty() ||
            binding.GetRelativeSource();
        if (isExplicitSource && source == nullptr) {
            continue;
        }

        Data::MetadataBindingDescriptor descriptor;
        descriptor.metadata = bindings->Metadata();
        descriptor.source = source;
        descriptor.target = &object;
        descriptor.targetProperty = setter.property;
        if (!isExplicitSource) {
            descriptor.dataContextProperty =
                FrameworkElement::DataContextProperty.Handle();
            descriptor.dataContextOwner = &object;
        }
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
        setterBindings_.PushBack(
            {&object, attached.Value()});
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


Base::Result<void> SealStyle(
    Style& style,
    const Meta::DependencyPropertyRegistry& properties) noexcept {
    return style.Seal(properties);
}

Base::Span<const StyleSetter> StyleRuntimeSetters(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Setters()
        : Base::Span<const StyleSetter>{};
}

Base::Span<const TriggerPlan> StyleRuntimeTriggers(
    const Style& style) noexcept {
    return style.program_ != nullptr
        ? style.program_->Triggers()
        : Base::Span<const TriggerPlan>{};
}

Base::Result<void> ApplyStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return Style::Program::ApplySetters(style, object, values);
}

Base::Result<void> ClearStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return Style::Program::ClearSetters(style, object, values);
}

StyleEngine::StyleEngine(
    EffectiveValueEngine& values,
    DependencyPropertyRegistry& properties) noexcept
    : providerSession_(values),
      values_(&providerSession_),
      properties_(&properties),
      applications_(),
      objectIndexMap_(),
      triggerEngine_(new TriggerEngine(
          *values_, *properties_, applications_)) {}

StyleEngine::~StyleEngine() noexcept {
    delete triggerEngine_;
}

void StyleEngine::SetTriggerActionHandler(
    TriggerActionHandler handler, void* context) noexcept {
    triggerEngine_->SetTriggerActionHandler(handler, context);
}

Base::Result<std::uint32_t> StyleEngine::Flush() noexcept {
    if (triggerEngine_ == nullptr) {
        return 0U;
    }
    return triggerEngine_->FlushPendingTriggerEvaluations();
}

const Base::Status& StyleEngine::LastActionStatus() const noexcept {
    return triggerEngine_->LastActionStatus();
}


} // namespace Aero

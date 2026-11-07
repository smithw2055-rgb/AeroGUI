#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/triggers/TriggerEngine.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/data/BindingCommon.hpp"
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Style.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Value.hpp>
#include <Aero/UIElement.hpp>

#include <new>
#include "gui/core/Describe.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

void Element::OnBlendingModeChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
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
    element->SetTransform3D(element->GetValue(Element::Transform3DProperty));
}

void TextProperties::OnCompatibilityPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    // Fast-path for standard controls: direct static property handles avoid runtime reflection lookup
    if (args.GetProperty() == TextProperties::PlaceholderProperty.Handle()) {
        if (auto* tb = ::Aero::TryCast<Controls::TextBox>(&object)) {
            tb->SetValue(Controls::TextBox::PlaceholderProperty.Handle(), args.GetNewValue());
            return;
        }
        if (auto* pb = ::Aero::TryCast<Controls::PasswordBox>(&object)) {
            pb->SetValue(Controls::PasswordBox::PlaceholderProperty.Handle(), args.GetNewValue());
            return;
        }
    } else if (args.GetProperty() == TextProperties::StrokeProperty.Handle()) {
        if (auto* tb = ::Aero::TryCast<Controls::TextBlock>(&object)) {
            tb->SetValue(Controls::TextBlock::StrokeProperty.Handle(), args.GetNewValue());
            return;
        }
    } else if (args.GetProperty() == TextProperties::StrokeThicknessProperty.Handle()) {
        if (auto* tb = ::Aero::TryCast<Controls::TextBlock>(&object)) {
            tb->SetValue(Controls::TextBlock::StrokeThicknessProperty.Handle(), args.GetNewValue());
            return;
        }
    }

    // Dynamic fallback for custom/other types
    const Meta::DependencyProperty* source =
        DependencyObjectAccess::PropertyRegistry((object)).Find(args.GetProperty());
    if (source == nullptr) return;

    const Meta::PropertyInfo* targetInfo =
        DependencyObjectAccess::PropertyRegistry((object)).Types().FindProperty(
            object.RuntimeType(), source->Name(), false);
    if (targetInfo == nullptr ||
        targetInfo->Id() == source->Handle().value) {
        return;
    }
    const Meta::DependencyProperty* target =
        DependencyObjectAccess::PropertyRegistry((object)).Find(
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
        DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
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
    const DependencyPropertyRegistry*,
    TypeId derived,
    TypeId expectedBase) noexcept {
    if (derived == expectedBase ||
        types.IsDerivedFrom(derived, expectedBase)) {
        return true;
    }
    if (expectedBase == Controls::ContentControl::StaticTypeId() &&
        (derived == Controls::HeaderedItemsControl::StaticTypeId() ||
         types.IsDerivedFrom(derived, Controls::HeaderedItemsControl::StaticTypeId()))) {
        return true;
    }
    return false;
}

Base::Result<PropertyValue> NormalizeStyleValue(
    const Meta::DependencyPropertyRegistry& properties,
    const DependencyProperty& property,
    const PropertyValue& value) noexcept {
    (void)properties;
    if (value.Kind() == Meta::ValueKind::Object) {
        const auto& asObj = value.AsObject();
        if (value.IsNullObject() || asObj.Get() == nullptr) {
            return PropertyValue::NullObject(property.ValueType());
        }
        Base::Ref<Base::Object> obj = asObj;
        if (obj.Get() != nullptr) {
            Meta::TypeId rt = obj->RuntimeType();
            return PropertyValue::FromObject(rt, std::move(obj));
        }
    }
    if (value.Type() != property.ValueType()) {
        Base::Result<PropertyValue> coerced =
            ::Aero::NormalizeValueForProperty(
                Meta::CurrentObjectFactory().metadata, property, value);
        if (coerced) {
            return coerced;
        }
    }
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
        : setters(&Base::GetDefaultAllocator()),
          triggers(&Base::GetDefaultAllocator()) {}
    Program(Program&&) noexcept = default;
    Program& operator=(Program&&) noexcept = default;
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    TypeId TargetType() const noexcept { return targetType; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return frozen
            ? Base::Span<const StyleSetter>{setters.Data(), setters.Size()}
            : Base::Span<const StyleSetter>{};
    }
    Base::Span<const TriggerPlan> Triggers() const noexcept {
        return frozen
            ? Base::Span<const TriggerPlan>{triggers.Data(), triggers.Size()}
            : Base::Span<const TriggerPlan>{};
    }
    std::uint64_t TriggerPropertyMask() const noexcept {
        return triggerPropertyMask;
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
    void Reset() noexcept;

    TypeId targetType = InvalidTypeId;
    Base::Vector<StyleSetter> setters;
    Base::Vector<TriggerPlan> triggers;
    std::uint64_t triggerPropertyMask = 0ULL;
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
    triggerPropertyMask = 0ULL;
    for (const TriggerPlan& trigger : triggers) {
        if (!trigger.IsBindingTrigger()) {
            triggerPropertyMask |= PropertyTriggerMask(trigger.property);
        }
    }
    frozen = true;
    return {};
}

void Style::Program::Reset() noexcept {
    targetType = InvalidTypeId;
    setters.Clear();
    triggers.Clear();
    triggerPropertyMask = 0ULL;
    frozen = false;
}

Base::Result<void> Style::Program::AddAuthoredSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StyleProgram is already frozen");
    }
    for (const StyleSetter& setter : setters) {
        if (setter.property == property) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Style already has a setter for this property");
        }
    }
    setters.PushBack({property, value});
    return {};
}

Base::Result<void> Style::Program::AddAuthoredTrigger(
    TriggerPlan trigger) noexcept {
    if (frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StyleProgram is already frozen");
    }
    triggers.PushBack(std::move(trigger));
    return {};
}

void Setter::ReleaseAuthored() noexcept {
    propertyName_.Clear();
    targetName_.Clear();
    authoredValue_ = PropertyValue{};
}

void Setter::SetPropertyName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return;
    }
    static_cast<void>(propertyName_.Assign(value));
}

void Setter::SetTargetName(
    Base::StringView value) noexcept {
    if (value.Empty()) {
        return;
    }
    static_cast<void>(targetName_.Assign(value));
}

void Setter::SetAuthoredValue(
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) return;
    authoredValue_ = value;
}

bool Setter::Resolve(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (!property.IsValid() || value.IsUnset()) {
        return false;
    }
    SetProperty(property);
    SetValue(value);
    return true;
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
      resources_() {
    static_assert(sizeof(Style::Program) <= sizeof(programStorage_),
        "programStorage_ is too small for Style::Program");
    new (programStorage_) Style::Program();
}

Style::~Style() {
    GetProgram().~Program();
}

TypeId Style::GetTargetType() const noexcept {
    return sealed_
        ? GetProgram().TargetType()
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
    GetProgram().AddAuthoredSetter(property, value);
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
        GetProgram().AddAuthoredTrigger(std::move(trigger));
    if (!planned) { AERO_ASSERT(false); return; }
}

void Style::AddTrigger(
    const Trigger& trigger) noexcept {
    if (sealed_) { AERO_ASSERT(false); return; }
    if (!trigger.property_.IsValid() || trigger.value_.IsUnset()) { AERO_ASSERT(false); return; }
    TriggerPlan plan;
    plan.property = trigger.property_;
    plan.value = trigger.value_;
    for (const Base::Ref<Setter>& authored : trigger.GetAuthoredSetters()) {
        if (!authored || !authored->GetProperty().IsValid() ||
            authored->GetValue().IsUnset()) { AERO_ASSERT(false); return; }
        plan.setters.PushBack({
            authored->GetProperty(), authored->GetValue()});
    }
    plan.enterActions.Append(
        trigger.GetEnterActions());
    plan.exitActions.Append(trigger.GetExitActions());
    Base::Result<void> planned =
        GetProgram().AddAuthoredTrigger(std::move(plan));
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
        GetProgram().AddAuthoredTrigger(std::move(plan));
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
        GetProgram().AddAuthoredTrigger(std::move(plan));
    if (!planned) { AERO_ASSERT(false); return; }
}

Base::Result<void> Style::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
    if (sealed_) {
        return {};
    }
    Program& program = GetProgram();
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

    auto resolveStyleProperty = [&](Base::StringView propName) noexcept -> DependencyPropertyHandle {
        if (propName.Empty()) return {};
        const Meta::DependencyProperty* resolved = properties.Find(targetType_, propName);
        if (resolved == nullptr) {
            std::uint32_t dot = UINT32_MAX;
            for (std::uint32_t i = 0U; i < propName.SizeBytes(); ++i) {
                if (propName[i] == '.') dot = i;
            }
            if (dot != UINT32_MAX && dot > 0U && dot + 1U < propName.SizeBytes()) {
                Base::StringView ownerName = propName.Substr(0U, dot);
                const Base::StringView unqualifiedName = propName.Substr(dot + 1U, propName.SizeBytes() - dot - 1U);
                for (std::uint32_t i = 0U; i < ownerName.SizeBytes(); ++i) {
                    if (ownerName[i] == ':') {
                        ownerName = ownerName.Substr(i + 1U, ownerName.SizeBytes() - i - 1U);
                        break;
                    }
                }
                const Meta::TypeInfo* ownerType = properties.Types().FindType(Meta::AeroNamespaceUri(), ownerName);
                if (ownerType != nullptr) {
                    resolved = properties.Find(ownerType->Id(), unqualifiedName);
                }
                if (resolved == nullptr) {
                    for (const Meta::TypeInfo& type : properties.Types().Types()) {
                        if (type.Name() == ownerName) {
                            resolved = properties.Find(type.Id(), unqualifiedName);
                            if (resolved != nullptr) break;
                        }
                    }
                }
                if (resolved == nullptr) {
                    resolved = properties.Find(targetType_, unqualifiedName);
                }
            }
        }
        return resolved != nullptr ? resolved->Handle() : DependencyPropertyHandle{};
    };

    for (const Base::Ref<SetterBase>& authored : authoredSetterObjects_) {
        Setter* setter = ::Aero::TryCast<Setter>(authored.Get());
        if (setter == nullptr) {
            continue;
        }
        DependencyPropertyHandle propHandle = setter->GetProperty();
        if (!propHandle.IsValid() && !setter->GetPropertyName().Empty()) {
            propHandle = resolveStyleProperty(setter->GetPropertyName());
            if (propHandle.IsValid()) {
                setter->SetProperty(propHandle);
            }
        }
        if (propHandle.IsValid()) {
            const PropertyValue& val = setter->GetValue();
            if (!val.IsUnset()) {
                bool exists = false;
                for (const StyleSetter& ss : program.setters) {
                    if (ss.property == propHandle) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    program.setters.PushBack({propHandle, val});
                }
            }
        }
    }

    auto appendTriggerSetters = [&](Span<const Base::Ref<Setter>> setters, TriggerPlan& plan) noexcept {
        for (const Base::Ref<Setter>& s : setters) {
            if (!s) continue;
            DependencyPropertyHandle sp = s->GetProperty();
            if (!sp.IsValid() && !s->GetPropertyName().Empty()) {
                sp = resolveStyleProperty(s->GetPropertyName());
            }
            if (sp.IsValid() && !s->GetValue().IsUnset()) {
                PropertyValue sVal = s->GetValue();
                const Meta::DependencyProperty* spProp = properties.Find(sp);
                if (spProp != nullptr && !IsDeferredBindingSetterValue(sVal)) {
                    auto norm = NormalizeStyleValue(properties, *spProp, sVal);
                    if (norm) sVal = norm.Value();
                }
                plan.setters.PushBack({sp, sVal});
            }
        }
    };

    for (const Base::Ref<TriggerBase>& entry : authoredTriggerObjects_) {
        if (Trigger* trigger = ::Aero::TryCast<Trigger>(entry.Get())) {
            TriggerPlan plan;
            plan.property = trigger->GetProperty();
            if (!plan.property.IsValid() && !trigger->GetPropertyName().Empty()) {
                plan.property = resolveStyleProperty(trigger->GetPropertyName());
            }
            plan.value = trigger->GetValue();
            if (plan.property.IsValid()) {
                const Meta::DependencyProperty* condProp = properties.Find(plan.property);
                if (condProp != nullptr) {
                    auto norm = NormalizeStyleValue(properties, *condProp, plan.value);
                    if (norm) plan.value = norm.Value();
                }
            }
            appendTriggerSetters(trigger->GetAuthoredSetters(), plan);
            plan.enterActions.Append(trigger->GetEnterActions());
            plan.exitActions.Append(trigger->GetExitActions());
            if (plan.property.IsValid() && !plan.value.IsUnset()) {
                program.triggers.PushBack(std::move(plan));
            }
        } else if (DataTrigger* dataTrigger = ::Aero::TryCast<DataTrigger>(entry.Get())) {
            if (dataTrigger->GetBinding() && !dataTrigger->GetValue().IsUnset()) {
                TriggerPlan plan;
                plan.binding = dataTrigger->GetBinding();
                plan.value = dataTrigger->GetValue();
                appendTriggerSetters(dataTrigger->GetAuthoredSetters(), plan);
                plan.enterActions.Append(dataTrigger->GetEnterActions());
                plan.exitActions.Append(dataTrigger->GetExitActions());
                if (!plan.setters.Empty() || !plan.enterActions.Empty() || !plan.exitActions.Empty()) {
                    program.triggers.PushBack(std::move(plan));
                }
            }
        } else if (MultiDataTrigger* multiDataTrigger = ::Aero::TryCast<MultiDataTrigger>(entry.Get())) {
            if (!multiDataTrigger->GetConditions().Empty()) {
                TriggerPlan plan;
                bool first = true;
                bool validConditions = true;
                for (const Base::Ref<Condition>& condition : multiDataTrigger->GetConditions()) {
                    if (!condition || !condition->GetBinding() || condition->GetValue().IsUnset()) {
                        validConditions = false;
                        break;
                    }
                    if (first) {
                        plan.binding = condition->GetBinding();
                        plan.value = condition->GetValue();
                        first = false;
                    } else {
                        TriggerBindingCondition extra;
                        extra.binding = condition->GetBinding();
                        extra.value = condition->GetValue();
                        plan.extraBindings.PushBack(std::move(extra));
                    }
                }
                if (validConditions && !first) {
                    appendTriggerSetters(multiDataTrigger->GetAuthoredSetters(), plan);
                    plan.enterActions.Append(multiDataTrigger->GetEnterActions());
                    plan.exitActions.Append(multiDataTrigger->GetExitActions());
                    if (!plan.setters.Empty() || !plan.enterActions.Empty() || !plan.exitActions.Empty()) {
                        program.triggers.PushBack(std::move(plan));
                    }
                }
            }
        }
    }

    Base::Vector<StyleSetter> next;
    const std::size_t estimatedCapacity =
        (basedOn_ != nullptr ? Style::Program::RuntimeSetters(*basedOn_).Size() : 0U) +
        program.setters.Size();
    next.Reserve(static_cast<Base::Vector<StyleSetter>::SizeType>(estimatedCapacity));
    if (basedOn_ != nullptr) {
        next.Append(
            Style::Program::RuntimeSetters(*basedOn_));
    }
    for (const StyleSetter& setter : program.setters) {
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
                NormalizeStyleValue(properties, *property, normalizedValue);
            if (!normalized) return normalized.GetStatus();
            normalizedValue = std::move(normalized).Value();
            Base::Result<void> validValue = properties.ValidateValueFor(
                setter.property, targetType_, normalizedValue);
            if (!validValue) {
                return validValue.GetStatus();
            }
        }
        const DependencyPropertyHandle canonicalHandle = property->Handle();
        bool replaced = false;
        for (StyleSetter& inherited : next) {
            if (inherited.property == canonicalHandle) {
                inherited.property = canonicalHandle;
                inherited.value = normalizedValue;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            next.PushBack({
                canonicalHandle, normalizedValue});
        }
    }
    Base::Vector<TriggerPlan> nextTriggers;
    const std::size_t estimatedTriggerCapacity =
        (basedOn_ != nullptr ? Style::Program::RuntimeTriggers(*basedOn_).Size() : 0U) +
        program.triggers.Size();
    nextTriggers.Reserve(static_cast<Base::Vector<TriggerPlan>::SizeType>(estimatedTriggerCapacity));
    if (basedOn_ != nullptr) {
        nextTriggers.Append(Style::Program::RuntimeTriggers(*basedOn_));
    }
    for (TriggerPlan& trigger : program.triggers) {
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
            trigger.property = condition->Handle();
            Base::Result<void> validCondition = properties.ValidateValueFor(
                trigger.property, targetType_, trigger.value);
            if (!validCondition) return validCondition.GetStatus();
        }
        for (std::uint32_t index = 0U;
             index < trigger.setters.Size();
             ++index) {
            StyleTriggerSetter& setter = trigger.setters[index];
            const Meta::DependencyProperty* property =
                properties.Find(setter.property);
            if (property == nullptr ||
                property->MetadataFor(targetType_) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style trigger setter does not apply to TargetType");
            }
            setter.property = property->Handle();
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
        nextTriggers.PushBack(std::move(trigger));
    }
    Base::Result<void> frozenProgram = program.Freeze(
        targetType_, std::move(next), std::move(nextTriggers));
    if (!frozenProgram) return frozenProgram.GetStatus();
    resources_.Seal();
    std::uint32_t keptEventSetters = 0U;
    for (std::uint32_t index = 0U; index < authoredSetterObjects_.Size(); ++index) {
        if (::Aero::TryCast<EventSetter>(authoredSetterObjects_[index].Get()) != nullptr) {
            if (keptEventSetters != index) {
                authoredSetterObjects_[keptEventSetters] =
                    std::move(authoredSetterObjects_[index]);
            }
            ++keptEventSetters;
        }
    }
    if (keptEventSetters == 0U) {
        authoredSetterObjects_ = Base::Vector<Base::Ref<SetterBase>>(
            &authoredSetterObjects_.Allocator());
    } else {
        static_cast<void>(authoredSetterObjects_.Resize(keptEventSetters));
    }
    // Retain immutable EventTrigger declarations for per-element routed-event
    // subscriptions. Property/DataTrigger/MultiDataTrigger plans are compiled
    // into program_ and their authored objects can now be released.
    std::uint32_t keptEventTriggers = 0U;
    for (std::uint32_t index = 0U; index < authoredTriggerObjects_.Size(); ++index) {
        if (authoredTriggerObjects_[index] &&
            ::Aero::TryCast<SetterTriggerBase>(authoredTriggerObjects_[index].Get()) == nullptr) {
            if (keptEventTriggers != index) {
                authoredTriggerObjects_[keptEventTriggers] =
                    std::move(authoredTriggerObjects_[index]);
            }
            ++keptEventTriggers;
        }
    }
    if (keptEventTriggers == 0U) {
        authoredTriggerObjects_ = Base::Vector<Base::Ref<TriggerBase>>(
            &authoredTriggerObjects_.Allocator());
    } else {
        static_cast<void>(authoredTriggerObjects_.Resize(keptEventTriggers));
    }
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
    return style.GetProgram().Setters();
}

Base::Span<const TriggerPlan> Style::Program::RuntimeTriggers(
    const Style& style) noexcept {
    return style.GetProgram().Triggers();
}

} // namespace Aero

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

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
    const auto authoredSetters = style.GetAuthoredSetters();
    if (!authoredSetters.Empty()) {
        UIElement* element = ::Aero::TryCast<UIElement>(&object);
        if (element != nullptr) {
            for (const Base::Ref<SetterBase>& authored : authoredSetters) {
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
    const auto authoredSetters = style.GetAuthoredSetters();
    if (!authoredSetters.Empty()) {
        UIElement* element = ::Aero::TryCast<UIElement>(&object);
        if (element != nullptr) {
            for (const Base::Ref<SetterBase>& authored : authoredSetters) {
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
    triggerEngine_->EnableDataBindPhase(object);
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    const std::uint32_t existing = FindApplication(object);
    const bool requiresSubscription =
        existing == UINT32_MAX ||
        applications_[existing].style != &style;
    if (existing != UINT32_MAX && applications_[existing].style != &style) {
        const Style& previousStyle = *applications_[existing].style;
        if (!Style::Program::RuntimeTriggers(previousStyle).Empty()) {
            triggerEngine_->RemovePendingTriggerEvaluation(object);
            triggerEngine_->UnsubscribeTriggers(object, previousStyle);
            triggerEngine_->ClearTriggerSetters(object, previousStyle);
        }
        Base::Result<void> cleared = ClearSetters(object, previousStyle);
        if (!cleared) {
            return cleared.GetStatus();
        }
    }
    Base::Result<void> setters =
        Style::Program::ApplySetters(style, object, *values_);
    if (!setters) {
        return setters.GetStatus();
    }
    const std::uint32_t triggerCount =
        Style::Program::RuntimeTriggers(style).Size();
    if (existing == UINT32_MAX) {
        StyleApplication application;
        application.object = &object;
        application.style = &style;
        application.triggerPropertyMask = StyleRuntimeTriggerMask(style);
        application.triggersInitialized = (triggerCount != 0U);
        const std::uint32_t newIndex = applications_.Size();
        applications_.PushBack(
                std::move(application));
        object.SetStyleApplicationIndex(newIndex);
    } else if (requiresSubscription) {
        StyleApplication& application = applications_[existing];
        application.style = &style;
        application.triggerPropertyMask = StyleRuntimeTriggerMask(style);
        application.triggerStatesMask = 0ULL;
        application.bindingTriggerStatesMask = 0ULL;
        application.bindingTriggerKnownMask = 0ULL;
        application.triggersInitialized = (triggerCount != 0U);
    }
    if (requiresSubscription) {
        Base::Result<void> attached = AttachSetterBindings(object, style);
        if (!attached) return attached.GetStatus();
        if (triggerCount != 0U) {
            triggerEngine_->SubscribeTriggers(object, style);
        }
    }
    if (triggerCount != 0U) {
        triggerEngine_->EvaluateTriggers(object, style);
    }
    return {};
}

void StyleEngine::SetBindingTriggerState(
    DependencyObject& object,
    const Style& style,
    std::uint32_t triggerIndex,
    bool active) noexcept {
    triggerEngine_->SetBindingTriggerState(
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
    if (!Style::Program::RuntimeTriggers(*actual).Empty()) {
        triggerEngine_->UnsubscribeTriggers(object, *actual);
        triggerEngine_->ClearTriggerSetters(object, *actual);
    }
    Base::Result<void> cleared = ClearSetters(object, *actual);
    if (!cleared) {
        return cleared.GetStatus();
    }
    if (existing != UINT32_MAX) {
        triggerEngine_->RemovePendingTriggerEvaluation(object);
        object.SetStyleApplicationIndex(UINT32_MAX);
        if (existing + 1U != applications_.Size()) {
            applications_[existing] = std::move(applications_[applications_.Size() - 1U]);
            if (applications_[existing].object != nullptr) {
                applications_[existing].object->SetStyleApplicationIndex(existing);
            }
        }
        applications_.PopBack();
    }
    return {};
}

bool StyleEngine::DetachObject(
    DependencyObject& object) noexcept {
    const std::uint32_t existing = FindApplication(object);
    if (existing == UINT32_MAX) {
        return false;
    }
    const Style& style = *applications_[existing].style;
    if (!Style::Program::RuntimeTriggers(style).Empty()) {
        triggerEngine_->RemovePendingTriggerEvaluation(object);
        triggerEngine_->UnsubscribeTriggers(object, style);
        triggerEngine_->ClearTriggerSetters(object, style);
    }
    (void)ClearSetters(object, style);
    object.SetStyleApplicationIndex(UINT32_MAX);
    if (existing + 1U != applications_.Size()) {
        applications_[existing] = std::move(applications_[applications_.Size() - 1U]);
        if (applications_[existing].object != nullptr) {
            applications_[existing].object->SetStyleApplicationIndex(existing);
        }
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
    const std::uint32_t idx = object.StyleApplicationIndex();
    return (idx < applications_.Size() && applications_[idx].object == &object)
        ? idx
        : UINT32_MAX;
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
    BindingEngine* bindings = nullptr;
    bool resolvedBindings = false;
    for (const StyleSetter& setter : Style::Program::RuntimeSetters(style)) {
        if (!IsDeferredBindingSetterValue(setter.value)) {
            continue;
        }
        if (setter.value.IsDynamicResource()) {
            Base::StringView dynamicKey = setter.value.AsDynamicResourceKey();
            if (effectiveValuesEngine_ != nullptr) {
                Base::Result<void> attached =
                    Markup::AttachDeferredStyleDynamicResource(
                        *effectiveValuesEngine_,
                        object,
                        setter.property,
                        dynamicKey);
                if (!attached) return attached.GetStatus();
            }
            continue;
        }
        if (!resolvedBindings) {
            bindings = ElementTree::BindingsOf(object);
            resolvedBindings = true;
        }
        Base::Ref<Base::Object> stored = setter.value.AsObject();
        if (bindings == nullptr || bindings->Metadata() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Style Binding setters require a mounted View binding engine");
        }
        if (!stored || stored->RuntimeType() != Data::Binding::StaticTypeId()) {
            continue;
        }
        auto& binding = static_cast<Data::Binding&>(*stored);
        Base::Object* source = nullptr;
        Base::Result<Base::Object*> codeSource = Data::ResolveCodeBindingSource(
            binding, object, bindings->Metadata(), nullptr, nullptr, /*startAtTarget=*/false);
        if (codeSource) {
            source = codeSource.Value();
        }
        const bool isExplicitSource = binding.GetSource() ||
            !binding.GetElementName().Empty() ||
            binding.GetRelativeSource();
        if (isExplicitSource && source == nullptr) {
            continue;
        }

        Data::MetadataBindingDescriptor descriptor =
            Data::MetadataBindingDescriptor::FromBinding(
                binding,
                object,
                setter.property,
                bindings->Metadata(),
                source);
        Base::Result<Data::BindingHandle> attached =
            bindings->Attach(descriptor);
        if (!attached) return attached.GetStatus();
        const std::uint32_t appIndex = FindApplication(object);
        if (appIndex != UINT32_MAX) {
            applications_[appIndex].setterBindings.PushBack(
                attached.Value());
        }
    }
    return {};
}

void StyleEngine::DetachSetterBindings(DependencyObject& object) noexcept {
    const std::uint32_t appIndex = FindApplication(object);
    if (appIndex == UINT32_MAX ||
        applications_[appIndex].setterBindings.Empty()) {
        return;
    }
    Base::Vector<Data::BindingHandle> handles =
        std::move(applications_[appIndex].setterBindings);
    applications_[appIndex].setterBindings.Clear();
    BindingEngine* bindings = ElementTree::BindingsOf(object);
    if (bindings == nullptr) {
        return;
    }
    for (const Data::BindingHandle handle : handles) {
        if (handle.IsValid()) {
            static_cast<void>(bindings->Detach(handle));
        }
    }
}


Base::Result<void> StyleEngine::SealProgram(
    Style& style,
    const Meta::DependencyPropertyRegistry& properties) noexcept {
    return Style::Program::Seal(style, properties);
}

Base::Span<const StyleSetter> StyleEngine::ProgramSetters(
    const Style& style) noexcept {
    return style.GetProgram().Setters();
}

Base::Span<const TriggerPlan> StyleEngine::ProgramTriggers(
    const Style& style) noexcept {
    return style.GetProgram().Triggers();
}

std::uint64_t StyleEngine::ProgramTriggerMask(
    const Style& style) noexcept {
    return style.GetProgram().TriggerPropertyMask();
}

Base::Result<void> StyleEngine::ProgramApplySetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return Style::Program::ApplySetters(style, object, values);
}

Base::Result<void> StyleEngine::ProgramClearSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return Style::Program::ClearSetters(style, object, values);
}

Base::Result<void> SealStyle(
    Style& style,
    const Meta::DependencyPropertyRegistry& properties) noexcept {
    return StyleEngine::SealProgram(style, properties);
}

Base::Span<const StyleSetter> StyleRuntimeSetters(
    const Style& style) noexcept {
    return StyleEngine::ProgramSetters(style);
}

Base::Span<const TriggerPlan> StyleRuntimeTriggers(
    const Style& style) noexcept {
    return StyleEngine::ProgramTriggers(style);
}

std::uint64_t StyleRuntimeTriggerMask(
    const Style& style) noexcept {
    return StyleEngine::ProgramTriggerMask(style);
}

Base::Result<void> ApplyStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return StyleEngine::ProgramApplySetters(style, object, values);
}

Base::Result<void> ClearStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept {
    return StyleEngine::ProgramClearSetters(style, object, values);
}

StyleEngine::StyleEngine(
    EffectiveValueEngine& values,
    DependencyPropertyRegistry& properties) noexcept
    : providerSession_(values),
      values_(&providerSession_),
      effectiveValuesEngine_(&values),
      properties_(&properties),
      applications_(),
      triggerEngine_(new TriggerEngine(
          *values_, *properties_, applications_)) {
    effectiveValuesEngine_->SetStyleObjectDestroyedHook(
        &StyleEngine::OnObjectDestroyedHook, this);
}

StyleEngine::~StyleEngine() noexcept {
    if (effectiveValuesEngine_ != nullptr) {
        effectiveValuesEngine_->SetStyleObjectDestroyedHook(nullptr, nullptr);
    }
    for (StyleApplication& app : applications_) {
        if (app.object != nullptr) {
            app.object->SetStyleApplicationIndex(UINT32_MAX);
        }
    }
    delete triggerEngine_;
}

void StyleEngine::OnObjectDestroyedHook(
    void* context,
    DependencyObject& object) noexcept {
    static_cast<StyleEngine*>(context)->OnObjectDestroyed(object);
}

void StyleEngine::OnObjectDestroyed(
    DependencyObject& object) noexcept {
    const std::uint32_t existing = FindApplication(object);
    if (existing == UINT32_MAX) {
        object.SetStyleApplicationIndex(UINT32_MAX);
        return;
    }
    if (triggerEngine_ != nullptr) {
        triggerEngine_->RemovePendingTriggerEvaluation(object);
    }
    DetachSetterBindings(object);
    object.SetStyleApplicationIndex(UINT32_MAX);
    if (existing + 1U != applications_.Size()) {
        applications_[existing] = std::move(applications_[applications_.Size() - 1U]);
        if (applications_[existing].object != nullptr) {
            applications_[existing].object->SetStyleApplicationIndex(existing);
        }
    }
    applications_.PopBack();
}

void StyleEngine::SetTriggerActionHandler(
    TriggerActionHandler handler, void* context) noexcept {
    triggerEngine_->SetTriggerActionHandler(handler, context);
}

std::uint32_t StyleEngine::Flush() noexcept {
    if (triggerEngine_ == nullptr) {
        return 0U;
    }
    return triggerEngine_->FlushPendingTriggerEvaluations();
}

const Base::Status& StyleEngine::LastActionStatus() const noexcept {
    return triggerEngine_->LastActionStatus();
}


} // namespace Aero

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
namespace {

TypeReference GetStyleTargetType(
    const Style& style) noexcept {
    return {style.GetTargetType()};
}

void SetStyleTargetType(
    Style& style,
    TypeReference value) noexcept {
    // TargetType is authored as a TypeReference by the XAML schema.  Keep the
    // resolved runtime TypeId on the Style so implicit style keys remain
    // distinct (for example Label, ComboBox, and ComboBoxItem).
    (void)style.SetTargetType(value.type);
}

void SetStyleBasedOn(
    Style& style,
    Base::Ref<Style> value) noexcept {
    (void)style.SetBasedOn(Base::Ref<Base::Object>(
        std::move(value)));
}

void AddStyleSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    auto* setterBase = ::Aero::TryCast<SetterBase>(value.Get());
    if (setterBase == nullptr) {
        return;
    }
    Base::Ref<SetterBase> retained =
        Base::Ref<SetterBase>::TryFromBorrowed(*setterBase);
    if (!retained) {
        return;
    }
    static_cast<Style&>(owner).GetSetters().Add(
        std::move(retained));
}

void ClearStyleSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Style&>(owner).GetSetters().Clear();
    return;
}

void AddStyleTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    Base::Ref<TriggerBase> retained =
        Base::Ref<TriggerBase>::TryFromBorrowed(
            static_cast<TriggerBase&>(*value));
    if (!retained) {
        return;
    }
    static_cast<Style&>(owner).GetTriggers().Add(
        std::move(retained));
}

void ClearStyleTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Style&>(owner).GetTriggers().Clear();
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero {

AERO_DESCRIBE(Element) {
    using namespace Aero::Meta;
    Register<Element>(context, TypeFlags::Abstract)
            .Property(Element::PPAAInProperty, 0.0, AffectsRender)
            .Property(Element::PPAAOutProperty, 0.0, AffectsRender)
            .Property(Element::PPAAModeProperty, Base::String{}, AffectsRender)
            .Property(Element::IsFocusEngagedProperty, false, AffectsRender)
            .Property(Element::BlendingModeProperty, FrameworkPropertyMetadata(BlendMode::Normal, AffectsRender).Changed(&Element::OnBlendingModeChanged))
            .Property(Element::Transform3DProperty, FrameworkPropertyMetadata(Base::Ref<Media::Transform3D>{}, AffectsRender).Changed(&Element::OnTransform3DChanged));
}

AERO_DESCRIBE(TextProperties) {
    using namespace Aero::Meta;
    Register<TextProperties>(context, TypeFlags::Abstract)
            .Property(TextProperties::PasswordLengthProperty, std::uint32_t{0}, AffectsRender)
            .Property(TextProperties::PlaceholderProperty, FrameworkPropertyMetadata(Base::String{}, AffectsRender).Changed(&TextProperties::OnCompatibilityPropertyChanged))
            .Property(TextProperties::StrokeProperty, FrameworkPropertyMetadata(Value::NullObject(TypeOf<Base::Object>()), AffectsRender).Changed(&TextProperties::OnCompatibilityPropertyChanged))
            .Property(TextProperties::StrokeThicknessProperty, FrameworkPropertyMetadata(0.0, AffectsRender).Changed(&TextProperties::OnCompatibilityPropertyChanged));
}

AERO_DESCRIBE(RichText) {
    using namespace Aero::Meta;
    Register<RichText>(context, TypeFlags::Abstract)
            .Property(RichText::TextProperty, FrameworkPropertyMetadata(Base::String{}, AffectsMeasure).Changed(&RichText::OnTextChanged));
}

AERO_DESCRIBE(Style) {
    using namespace Aero::Meta;
    Register<Style>(context)
            .Property<TypeReference, &::Aero::MetadataSupport::GetStyleTargetType, &::Aero::MetadataSupport::SetStyleTargetType>("TargetType", PropertyFlags::None)
            .Property<Base::Ref<Style>, &::Aero::MetadataSupport::SetStyleBasedOn>("BasedOn", PropertyFlags::WriteOnly)
            .Property<Base::Ref<ResourceDictionary>, &Style::SetResources>("Resources", PropertyFlags::Structural)
            .Collection<TriggerBase>("Triggers", &::Aero::MetadataSupport::AddStyleTrigger, &::Aero::MetadataSupport::ClearStyleTriggers)
            .Content<Setter>("Setters", ContentKind::Collection, &::Aero::MetadataSupport::AddStyleSetter, &::Aero::MetadataSupport::ClearStyleSetters)
            .Factory();
}

} // namespace Aero


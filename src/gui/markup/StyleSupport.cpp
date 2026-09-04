#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include <cstdio>

// ===== StyleSupport =====



#include <Aero/Base/String.hpp>

#include <Aero/Controls.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/Transform.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Span.hpp>
#include <Aero/Documents/Inline.hpp>

#include <cstdio>
#include <new>
#include <utility>

namespace Aero::Markup {
using namespace ::Aero::Markup;

namespace {

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status MissingStyleProperty(
    const char* role,
    Base::StringView property,
    Meta::TypeId targetType,
    const Meta::TypeRegistry& types) noexcept {
    thread_local char message[384];
    const Meta::TypeInfo* target =
        types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr
        ? target->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Style %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        message);
}

const Meta::DependencyProperty* ResolveStyleProperty(
    const Meta::DependencyPropertyRegistry& properties,
    Meta::TypeId targetType,
    Base::StringView name) noexcept {
    const Meta::DependencyProperty* property =
        properties.Find(targetType, name);
    if (property != nullptr) return property;
    if (name == Base::StringView("ContextMenu")) {
        property = properties.Find(
            Controls::ContextMenuService::
                ContextMenuProperty.Handle());
        if (property != nullptr) return property;
    }
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < name.SizeBytes();
         ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX ||
        separator == 0U ||
        separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }
    Base::StringView ownerName = name.Substr(0U, separator);
    // Setter.Property is authored as text, so a qualified attached property
    // retains its XML prefix (for example aero:Element.Transform3D). The
    // namespace has already resolved to Aero's compatibility schema while the
    // object graph was parsed; strip the lexical prefix before metadata lookup.
    for (std::uint32_t index = 0U; index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(
                index + 1U,
                ownerName.SizeBytes() - index - 1U);
            break;
        }
    }
    const Meta::TypeInfo* owner =
        properties.Types().FindType(
            Meta::AeroNamespaceUri(),
            ownerName);
    const Base::StringView member = name.Substr(
        separator + 1U,
        name.SizeBytes() - separator - 1U);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), member);
        if (property != nullptr) return property;
    }
    // WPF-compatible style markup may qualify a property with a base class
    // while the concrete runtime registers the same inherited property on a
    // derived control type.
    return properties.Find(targetType, member);
}

Base::Result<Meta::PropertyValue> ToPropertyValue(
    const Meta::Value& value,
    Meta::TypeId expectedType) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML style value cannot be represented as a dependency-property value");
    }
    if (value.IsNullObject() && value.Type() != expectedType) {
        return Meta::PropertyValue::NullObject(expectedType);
    }
    return value;
}

Aero::ResourceDictionary* ResolveStyleResources(
    Base::Object& object,
    void*) noexcept {
    return object.RuntimeType() ==
            Aero::Style::StaticTypeId()
        ? &static_cast<Aero::Style&>(object).GetResources()
        : nullptr;
}

Base::Result<Aero::ResourceKey> ResolveStyleImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() != Aero::Style::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implicit Style key requires a Style object");
    }
    const Meta::TypeId target =
        static_cast<const Aero::Style&>(object).GetTargetType();
    if (target == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Implicit Style requires TargetType");
    }
    return Aero::ResourceKey::FromType(target);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const UiObjectModelOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlStyleSchemaFacet::Register(
    Schema& schema,
    Meta::TypeId styleType,
    Meta::TypeId setterType,
    Meta::DependencyPropertyHandle styleProperty,
    Meta::TypeId triggerType) noexcept {
    if (schema.IsFrozen() ||
        options_.properties == nullptr || !styleProperty.IsValid() ||
        !options_.properties->IsFrozen() || !options_.properties->Types().IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style extension registries are not ready");
    }
    if (schema_ != nullptr ||
        styleType == Meta::InvalidTypeId || setterType == Meta::InvalidTypeId) {
        return InvalidStyleXaml("XAML Style extension registration is invalid");
    }

    const Meta::TypeInfo* styleInfo = schema.Types().FindType(styleType);
    const Meta::TypeInfo* setterInfo = schema.Types().FindType(setterType);
    const Meta::TypeInfo* triggerInfo =
        triggerType != Meta::InvalidTypeId ? schema.Types().FindType(triggerType) : nullptr;
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Meta::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Meta::TypeFlags::ValueType) ||
        (triggerType != Meta::InvalidTypeId &&
         (triggerInfo == nullptr || HasTypeFlag(triggerInfo->Flags(), Meta::TypeFlags::ValueType)))) {
        return InvalidStyleXaml("XAML Style, Setter, and Trigger must be registered object types");
    }

    const Meta::PropertyInfo* targetType =
        schema.Types().FindProperty(styleType, Base::StringView("TargetType"), false);
    const Meta::PropertyInfo* basedOn =
        schema.Types().FindProperty(styleType, Base::StringView("BasedOn"), false);
    const Meta::PropertyInfo* setters =
        schema.Types().FindProperty(styleType, Base::StringView("Setters"), false);
    const Meta::PropertyInfo* property =
        schema.Types().FindProperty(setterType, Base::StringView("Property"), false);
    const Meta::PropertyInfo* value =
        schema.Types().FindProperty(setterType, Base::StringView("Value"), false);
    if (targetType == nullptr || basedOn == nullptr || setters == nullptr ||
        property == nullptr || value == nullptr ||
        targetType->ValueType() !=
            Meta::TypeOf<Meta::TypeReference>() ||
        basedOn->ValueType() != styleType || setters->ValueType() != setterType ||
        property->ValueType() !=
            Meta::TypeOf<Base::String>() ||
        value->ValueType() !=
            Meta::TypeOf<Meta::Value>() ||
        schema.Types().FindContentMember(styleType) != setters->Id()) {
        return InvalidStyleXaml("XAML Style metadata members are invalid");
    }

    const Meta::PropertyInfo* triggers = nullptr;
    const Meta::PropertyInfo* triggerProperty = nullptr;
    const Meta::PropertyInfo* triggerValue = nullptr;
    const Meta::PropertyInfo* triggerSetters = nullptr;
    if (triggerType != Meta::InvalidTypeId) {
        triggers = schema.Types().FindProperty(
            styleType, Base::StringView("Triggers"), false);
        triggerProperty = schema.Types().FindProperty(
            triggerType, Base::StringView("Property"), false);
        triggerValue = schema.Types().FindProperty(
            triggerType, Base::StringView("Value"), false);
        triggerSetters = schema.Types().FindProperty(
            triggerType, Base::StringView("Setters"), false);
        if (triggers == nullptr || triggerProperty == nullptr ||
            triggerValue == nullptr || triggerSetters == nullptr ||
            !schema.Types().IsAssignableFrom(
                triggers->ValueType(), triggerType) ||
            triggerProperty->ValueType() !=
                Meta::TypeOf<Base::String>() ||
            triggerValue->ValueType() !=
                Meta::TypeOf<Meta::Value>() ||
            triggerSetters->ValueType() != setterType) {
            return InvalidStyleXaml("XAML Trigger metadata members are invalid");
        }
    }

    const Meta::DependencyProperty* styleDependency = options_.properties->Find(styleProperty);
    if (styleDependency == nullptr || styleDependency->ValueType() != styleType) {
        return InvalidStyleXaml("XAML Style property does not accept the registered Style type");
    }

    schema_ = &schema;
    styleType_ = styleType;
    setterType_ = setterType;
    triggerType_ = triggerType;
    Base::Result<void> styleAdapter =
        SchemaPrivate::AddType(schema, {
        styleType_,
        nullptr,
        &EndStyleInit,
        nullptr,
        this,
        false,
        true,
        nullptr,
        nullptr,
        &ResolveStyleResources,
        nullptr,
        false,
        &ResolveStyleImplicitKey});
    if (!styleAdapter) return styleAdapter.GetStatus();
    return {};
}

Base::Result<Meta::PropertyValue> XamlStyleSchemaFacet::ConvertValueForProperty(
    const Meta::Value& value,
    Meta::TypeId targetType,
    Base::StringView propertyName) const noexcept {
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style conversion requires an initialized extension");
    }
    // Setter.Value="{Binding ...}" is a declaration. Converting it to the
    // target DP type (Canvas.Left is double) would drop the Binding and
    // fail style sealing, so ListBoxItem orbit placement never applied.
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        (value.Type() == Data::Binding::StaticTypeId() ||
         (value.AsObject() &&
          value.AsObject()->RuntimeType() ==
              Data::Binding::StaticTypeId()))) {
        return value;
    }
    const Meta::DependencyProperty* property =
        ResolveStyleProperty(
            *options_.properties,
            targetType,
            propertyName);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style property was not found on TargetType");
    }
    const Meta::Value* candidate = &value;
    Base::Result<Meta::Value> converted = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Style conversion was not attempted");
    if (candidate->Kind() == Meta::ValueKind::String) {
        converted = schema_->ConvertText(property->ValueType(), candidate->AsString());
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    if (property->ValueType() == Aero::Length::StaticTypeId() &&
        candidate->Type() == Meta::TypeOf<double>()) {
        Base::Result<double> numeric =
            Meta::ValueCodec<double>::Decode(*candidate);
        if (!numeric) return numeric.GetStatus();
        converted = Meta::ValueCodec<Aero::Length>::Encode(
            Aero::Length::Pixels(numeric.Value()));
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    if (property->ValueType() ==
            Media::Brush::StaticTypeId() &&
        candidate->Type() ==
            Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                *candidate);
        if (!color) return color.GetStatus();
        Base::Result<
            Base::Ref<Media::Brush>>
            brush =
                Media::MakeSolidColorBrush(
                    color.Value());
        if (!brush) return brush.GetStatus();
        return Meta::Value::FromObject(
            Media::Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    if (property->ValueType() == Meta::TypeOf<Base::Color>() &&
        candidate->Kind() == Meta::ValueKind::Object &&
        options_.properties->Types().IsDerivedFrom(
            candidate->Type(), Media::Brush::StaticTypeId())) {
        Base::Result<Base::Ref<Media::Brush>> brush =
            Meta::ValueCodec<Base::Ref<Media::Brush>>::Decode(
                *candidate);
        if (!brush) return brush.GetStatus();
        Media::Brush* source = brush.Value().Get();
        if (source == nullptr || source->RuntimeType() !=
                Media::SolidColorBrush::StaticTypeId()) {
            return InvalidStyleXaml(
                "Color-backed text property requires SolidColorBrush");
        }
        return Meta::ValueCodec<Base::Color>::Encode(
            static_cast<Media::SolidColorBrush*>(source)->GetColor());
    }
    return ToPropertyValue(*candidate, property->ValueType());
}

bool SetterValueIsPending(const Aero::Setter* setter) noexcept {
    return setter != nullptr &&
        !setter->GetPropertyName().Empty() &&
        setter->GetAuthoredValue().IsUnset();
}

bool StyleHasPendingResourceValues(const Aero::Style& style) noexcept {
    for (const Base::Ref<Aero::SetterBase>& entry :
         style.GetAuthoredSetters()) {
        if (SetterValueIsPending(
                ::Aero::TryCast<Aero::Setter>(entry.Get()))) {
            return true;
        }
    }
    for (const Base::Ref<Aero::TriggerBase>& entry :
         style.GetAuthoredTriggers()) {
        Aero::TriggerBase* authored = entry.Get();
        if (authored == nullptr) {
            continue;
        }
        if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::Trigger*>(authored);
            if (!trigger->GetPropertyName().Empty() &&
                trigger->GetAuthoredValue().IsUnset()) {
                return true;
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                if (SetterValueIsPending(setterEntry.Get())) {
                    return true;
                }
            }
            continue;
        }
        if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::DataTrigger*>(authored);
            if (trigger->GetBinding() &&
                trigger->GetAuthoredValue().IsUnset()) {
                return true;
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                if (SetterValueIsPending(setterEntry.Get())) {
                    return true;
                }
            }
            continue;
        }
        if (authored->RuntimeType() ==
                Aero::MultiDataTrigger::StaticTypeId()) {
            auto* trigger =
                static_cast<Aero::MultiDataTrigger*>(authored);
            for (const Base::Ref<Aero::Condition>& condition :
                 trigger->GetConditions()) {
                if (condition &&
                    condition->GetBinding() &&
                    condition->GetAuthoredValue().IsUnset()) {
                    return true;
                }
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                if (SetterValueIsPending(setterEntry.Get())) {
                    return true;
                }
            }
        }
    }
    return false;
}

Base::Result<void> XamlStyleSchemaFacet::FinalizeStyle(
    Aero::Style& style) noexcept {
    if (style.GetIsSealed()) {
        return {};
    }
    // Source-backed sibling dictionaries are committed after this EndInit
    // callback. A Setter.Value="{StaticResource ...}" whose key lives in a
    // theme/merged dictionary is queued as a deferred write; sealing now
    // would report "Style Setter requires Value". Leave the Style unsealed
    // until ObjectBuilder reapplies EndInit after those writes.
    if (StyleHasPendingResourceValues(style)) {
        return {};
    }
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style TargetType must be assigned before initialization completes");
    }
    if (style.GetTargetType() == Meta::InvalidTypeId) {
        if (!style.SetTargetType(
                Controls::Control::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style TargetType assignment failed");
        }
    }
    const Meta::TypeId targetType = style.GetTargetType();
    const Meta::TypeInfo* targetInfo =
        options_.properties->Types().FindType(targetType);
    if (targetInfo == nullptr ||
        HasTypeFlag(
            targetInfo->Flags(),
            Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style TargetType must identify an object type");
    }
    for (const Base::Ref<Aero::SetterBase>& entry :
         style.GetAuthoredSetters()) {
        Aero::Setter* setter =
            ::Aero::TryCast<Aero::Setter>(entry.Get());
        if (setter == nullptr) {
            continue;
        }
        if (setter->GetPropertyName().Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property");
        }
        if (setter->GetAuthoredValue().IsUnset()) {
            thread_local char message[256]{};
            const Base::StringView propertyName =
                setter->GetPropertyName();
            std::snprintf(
                message,
                sizeof(message),
                "Style Setter for '%.*s' requires Value",
                static_cast<int>(propertyName.SizeBytes()),
                propertyName.Data());
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                message);
        }
        const Meta::DependencyProperty* property =
            ResolveStyleProperty(
                *options_.properties,
                targetType,
                setter->GetPropertyName());
        if (property == nullptr) {
            return MissingStyleProperty(
                "Setter property",
                setter->GetPropertyName(),
                targetType,
                options_.properties->Types());
        }
        Base::Result<Meta::PropertyValue> value = ConvertValueForProperty(
            setter->GetAuthoredValue(),
            targetType,
            setter->GetPropertyName());
        if (!value) return value.GetStatus();
        Base::Result<void> resolved = setter->Resolve(
            property->Handle(), value.Value());
        if (!resolved) return resolved.GetStatus();
        Base::Result<void> added =
            style.AddSetter(*setter);
        if (!added) return added.GetStatus();
    }
    for (const Base::Ref<Aero::TriggerBase>& entry :
         style.GetAuthoredTriggers()) {
        Aero::TriggerBase* authored = entry.Get();
        if (authored == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Trigger cannot be null");
        }
        if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::Trigger*>(authored);
            if (!trigger->GetIsAuthored()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style Trigger requires Property and Value");
            }
            const Meta::DependencyProperty* condition =
                ResolveStyleProperty(
                    *options_.properties,
                    targetType,
                    trigger->GetPropertyName());
            if (condition == nullptr) {
                return MissingStyleProperty(
                    "Trigger property",
                    trigger->GetPropertyName(),
                    targetType,
                    options_.properties->Types());
            }
            Base::Result<Meta::PropertyValue> conditionValue =
                ConvertValueForProperty(
                    trigger->GetAuthoredValue(),
                    targetType,
                    trigger->GetPropertyName());
            if (!conditionValue) return conditionValue.GetStatus();
            trigger->SetProperty(condition->Handle());
            trigger->SetValue(conditionValue.Value());
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                Aero::Setter* setter = setterEntry.Get();
                if (setter == nullptr || !setter->GetIsAuthored()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style Trigger Setter requires Property and Value");
                }
                const Meta::DependencyProperty* property =
                    ResolveStyleProperty(
                        *options_.properties,
                        targetType,
                        setter->GetPropertyName());
                if (property == nullptr) {
                    return MissingStyleProperty(
                        "Trigger Setter property",
                        setter->GetPropertyName(),
                        targetType,
                        options_.properties->Types());
                }
                Base::Result<Meta::PropertyValue> value =
                    ConvertValueForProperty(
                        setter->GetAuthoredValue(),
                        targetType,
                        setter->GetPropertyName());
                if (!value) return value.GetStatus();
                Base::Result<void> resolved = setter->Resolve(
                    property->Handle(), value.Value());
                if (!resolved) return resolved.GetStatus();
                Base::Result<void> added = trigger->AddSetter(*setter);
                if (!added) return added.GetStatus();
            }
            Base::Result<void> added = style.AddTrigger(*trigger);
            if (!added) return added.GetStatus();
            continue;
        }
        if (options_.properties->Types().IsDerivedFrom(
                authored->RuntimeType(),
                Media::Animation::EventTrigger::StaticTypeId())) {
            auto* trigger =
                static_cast<Media::Animation::EventTrigger*>(authored);
            if (trigger->GetRoutedEvent().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style EventTrigger requires RoutedEvent");
            }
            // Event triggers are immutable declarations. The View creates a
            // routed-event subscription per styled element after mounting.
            continue;
        }
        if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::DataTrigger*>(authored);
            if (!trigger->GetBinding() ||
                trigger->GetAuthoredValue().IsUnset() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger requires Binding, Value, and Setters");
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                Aero::Setter* setter = setterEntry.Get();
                if (setter == nullptr || !setter->GetIsAuthored()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style DataTrigger Setter requires Property and Value");
                }
                const Meta::DependencyProperty* property =
                    ResolveStyleProperty(
                        *options_.properties,
                        targetType,
                        setter->GetPropertyName());
                if (property == nullptr) {
                    return MissingStyleProperty(
                        "DataTrigger Setter property",
                        setter->GetPropertyName(),
                        targetType,
                        options_.properties->Types());
                }
                Base::Result<Meta::PropertyValue> value =
                    ConvertValueForProperty(
                        setter->GetAuthoredValue(),
                        targetType,
                        setter->GetPropertyName());
                if (!value) return value.GetStatus();
                Base::Result<void> resolved = setter->Resolve(
                    property->Handle(), value.Value());
                if (!resolved) return resolved.GetStatus();
            }
            Base::Result<void> added = style.AddTrigger(*trigger);
            if (!added) return added.GetStatus();
            continue;
        }
        if (authored->RuntimeType() ==
                Aero::MultiDataTrigger::StaticTypeId()) {
            auto* trigger =
                static_cast<Aero::MultiDataTrigger*>(authored);
            if (trigger->GetConditions().Empty() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style MultiDataTrigger requires Conditions and Setters");
            }
            for (const Base::Ref<Aero::Condition>& condition :
                 trigger->GetConditions()) {
                if (!condition || !condition->GetBinding() ||
                    condition->GetAuthoredValue().IsUnset()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style MultiDataTrigger Condition requires Binding and Value");
                }
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 trigger->GetAuthoredSetters()) {
                Aero::Setter* setter = setterEntry.Get();
                if (setter == nullptr || !setter->GetIsAuthored()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style MultiDataTrigger Setter requires Property and Value");
                }
                const Meta::DependencyProperty* property =
                    ResolveStyleProperty(
                        *options_.properties,
                        targetType,
                        setter->GetPropertyName());
                if (property == nullptr) {
                    return MissingStyleProperty(
                        "MultiDataTrigger Setter property",
                        setter->GetPropertyName(),
                        targetType,
                        options_.properties->Types());
                }
                Base::Result<Meta::PropertyValue> value =
                    ConvertValueForProperty(
                        setter->GetAuthoredValue(),
                        targetType,
                        setter->GetPropertyName());
                if (!value) return value.GetStatus();
                Base::Result<void> resolved = setter->Resolve(
                    property->Handle(), value.Value());
                if (!resolved) return resolved.GetStatus();
            }
            Base::Result<void> added = style.AddTrigger(*trigger);
            if (!added) return added.GetStatus();
            continue;
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Style trigger type is not supported");
    }
    return Aero::StylePrivate::Seal(
        style, options_.properties);
}

Base::Result<void> XamlStyleSchemaFacet::EndStyleInit(
    Base::Object& object,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style initialization requires an extension context");
    }
    return extension->FinalizeStyle(
        static_cast<Aero::Style&>(
            object));
}

} // namespace Aero::Markup

namespace Aero::Markup {

struct UiObjectModelState {
    explicit UiObjectModelState(
        const UiObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.metadata,
              *options.properties,
              options.allocator) {}

    XamlStyleSchemaFacet style;
    XamlTemplateSchemaFacet templates;
    bool registered = false;
};

static_assert(
    sizeof(UiObjectModelState) <= 4096,
    "UiObjectModel inline state storage is too small");
static_assert(
    alignof(UiObjectModelState) <= alignof(std::max_align_t),
    "UiObjectModel inline state alignment is insufficient");

UiObjectModel::UiObjectModel(
    const UiObjectModelOptions& options) noexcept
    : allocator_(options.allocator != nullptr
          ? options.allocator
          : &Base::GetDefaultAllocator()) {
    if (options.metadata == nullptr ||
        options.properties == nullptr) {
        return;
    }
    optionsValid_ = true;
    state_ = new (stateStorage_) UiObjectModelState(options);
}

UiObjectModel::~UiObjectModel() noexcept {
    if (state_ == nullptr) return;
    state_->~UiObjectModelState();
    state_ = nullptr;
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema) noexcept {
    UiObjectModelTypes types;
    types.style = Aero::Style::StaticTypeId();
    types.setter = Aero::Setter::StaticTypeId();
    types.trigger = Aero::Trigger::StaticTypeId();
    types.styleProperty =
        Aero::FrameworkElement::StyleProperty;
    return Register(schema, types);
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema,
    const UiObjectModelTypes& types) noexcept {
    if (!optionsValid_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML UI object model options are invalid");
    }
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML UI object model allocation failed");
    }
    if (state_->registered) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML UI object model is already registered");
    }
    Base::Result<void> registered =
        state_->style.Register(
            schema,
            types.style,
            types.setter,
            types.styleProperty,
            types.trigger);
    if (registered && types.includeTemplates) {
        registered = state_->templates.Register(schema);
    }
    if (registered) {
        state_->registered = true;
    }
    return registered;
}

} // namespace Aero::Markup



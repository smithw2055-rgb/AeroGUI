#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

// ===== StyleSupport =====



#include <Aero/Base/String.hpp>

#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/FrameworkElement.hpp>

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

Base::Result<void> XamlStyleSchemaFacet::FinalizeStyle(
    Aero::Style& style) noexcept {
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
    for (const Base::Ref<Aero::Setter>& entry :
         style.GetAuthoredSetters()) {
        Aero::Setter* setter = entry.Get();
        if (setter == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property and Value");
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
                    "Style Trigger requires Property, Value, and Setters");
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


// ===== TemplateSupport =====



#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include <Aero/Controls/ControlTemplate.hpp>






namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Meta;
using namespace Aero::Threading;


class CompiledTemplateProgramOwner
    : public Base::Object {
public:
    explicit CompiledTemplateProgramOwner(
        CompiledTemplateBlueprint blueprint) noexcept
        : blueprint_(std::move(blueprint)) {}
    ~CompiledTemplateProgramOwner() noexcept override = default;

    CompiledTemplateBlueprint& Blueprint() noexcept {
        return blueprint_;
    }

private:
    CompiledTemplateBlueprint blueprint_;
};

Base::Status InvalidTemplateXaml(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

bool TemplateHasTypeFlag(
    TypeFlags value,
    TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() ==
            ControlTemplate::StaticTypeId()) {
        return &static_cast<ControlTemplate&>(
            object).GetResources();
    }
    if (object.RuntimeType() ==
            DataTemplate::StaticTypeId()) {
        return &static_cast<DataTemplate&>(
            object).GetResources();
    }
    if (object.RuntimeType() ==
            ItemsPanelTemplate::StaticTypeId()) {
        return &static_cast<ItemsPanelTemplate&>(
            object).GetResources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Meta::TypeId key = Meta::InvalidTypeId;
    if (object.RuntimeType() == ControlTemplate::StaticTypeId()) {
        key = static_cast<const ControlTemplate&>(object).GetTargetType();
    } else if (object.RuntimeType() == DataTemplate::StaticTypeId()) {
        key = static_cast<const DataTemplate&>(object).GetDataType();
    }
    if (key == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

struct XamlTemplateSchemaFacetState {
    XamlTemplateSchemaFacetState(
        Meta::Registry& metadata,
        DependencyPropertyRegistry& dependencyProperties,
        Base::IAllocator& programAllocator) noexcept
        : allocator(&programAllocator),
          runtime(&metadata),
          properties(&dependencyProperties) {}

    Base::IAllocator* allocator = nullptr;
    Meta::Registry* runtime = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    Schema* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const ExtensionServices& services,
        void* context) noexcept {
        auto* self = static_cast<XamlTemplateSchemaFacetState*>(context);
        const TypeId type = object.RuntimeType();
        if (self == nullptr || self->runtime == nullptr ||
            self->properties == nullptr ||
            (type != ControlTemplate::StaticTypeId() &&
             type != DataTemplate::StaticTypeId() &&
             type != ItemsPanelTemplate::StaticTypeId()) ||
            services.deferredContentOwner != &object ||
            services.deferredContent == nullptr) {
            return InvalidTemplateXaml(
                "Template deferred-content scope is invalid");
        }
        if (type == ControlTemplate::StaticTypeId() ||
            type == DataTemplate::StaticTypeId()) {
            const Meta::TypeId targetType =
                type == ControlTemplate::StaticTypeId()
                ? static_cast<ControlTemplate&>(object)
                      .GetTargetType()
                : static_cast<DataTemplate&>(object)
                      .GetDataType();
            // A keyed WPF ControlTemplate may deliberately omit TargetType.
            // Its target is inferred from the Style/Setter that consumes it,
            // so only validate an explicitly authored type here. The apply
            // path still checks that the eventual target is a Control.
            if (targetType != Meta::InvalidTypeId) {
                const Meta::TypeInfo* targetInfo =
                    self->runtime->Types().FindType(targetType);
                if (targetInfo == nullptr ||
                    TemplateHasTypeFlag(
                        targetInfo->Flags(),
                        Meta::TypeFlags::ValueType)) {
                    return InvalidTemplateXaml(
                        "Template type constraint must identify an object type");
                }
                if (type == ControlTemplate::StaticTypeId() &&
                    !self->runtime->Types().IsDerivedFrom(
                        targetType,
                        Control::StaticTypeId())) {
                    return InvalidTemplateXaml(
                        "ControlTemplate TargetType is not a Control");
                }
            }
        }
        Base::Vector<DeferredContentEdge> edges(
            self->allocator);
        Base::Result<void> copied =
            services.deferredContent->CopyForOwner(
                object, edges);
        if (!copied) return copied.GetStatus();
        Base::Vector<DeferredBindingEdge> bindings(
            self->allocator);
        copied =
            services.deferredContent->
                CopyBindingsForOwner(
                    object, bindings);
        if (!copied) return copied.GetStatus();

        if (services.baseUri != nullptr) {
            Base::Result<void> baseUri;
            if (object.RuntimeType() ==
                    ControlTemplate::StaticTypeId()) {
                auto& templateValue = static_cast<ControlTemplate&>(object);
                if (::Aero::Controls::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = ::Aero::Controls::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       DataTemplate::StaticTypeId()) {
                auto& templateValue = static_cast<DataTemplate&>(object);
                if (::Aero::Controls::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = ::Aero::Controls::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       ItemsPanelTemplate::StaticTypeId()) {
                auto& templateValue = static_cast<ItemsPanelTemplate&>(object);
                if (::Aero::Controls::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = ::Aero::Controls::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
                }
            }
            if (!baseUri) return baseUri.GetStatus();
        }
        if (object.RuntimeType() ==
                DataTemplate::StaticTypeId() ||
            object.RuntimeType() ==
                ItemsPanelTemplate::StaticTypeId()) {
            const Base::Ref<Base::Object>* authored = nullptr;
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                authored =
                    &::Aero::Controls::TemplatePrivate::AuthoredVisualTree(static_cast<DataTemplate&>(object));
            } else {
                authored =
                    &::Aero::Controls::TemplatePrivate::AuthoredVisualTree(static_cast<ItemsPanelTemplate&>(object));
            }
            Base::Result<CompiledTemplateBlueprint>
                compiled =
                    CompileDeferredTemplateBlueprint(
                        *authored,
                        object.RuntimeType() ==
                                DataTemplate::StaticTypeId()
                            ? &::Aero::Controls::TemplatePrivate::AuthoredNames(static_cast<DataTemplate&>(object))
                            : nullptr,
                        {
                            edges.Data(),
                            edges.Size()},
                        {
                            bindings.Data(),
                            bindings.Size()},
                        *self->runtime,
                        *self->properties);
            if (!compiled) {
                return compiled.GetStatus();
            }
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                Base::Result<void> reserved =
                    compiled.Value().
                        dataTemplateTriggers.Reserve(
                            ::Aero::Controls::TemplatePrivate::AuthoredTriggers(dataTemplate).Size());
                if (!reserved) {
                    return reserved.GetStatus();
                }
                for (const Base::Ref<
                         Aero::TriggerBase>& trigger :
                     ::Aero::Controls::TemplatePrivate::AuthoredTriggers(dataTemplate)) {
                    Base::Result<void> retained =
                        compiled.Value().
                            dataTemplateTriggers.
                                PushBack(trigger);
                    if (!retained) {
                        return retained.GetStatus();
                    }
                }
            }
            Base::Result<Base::Ref<CompiledTemplateProgramOwner>>
                program =
                    Base::MakeRefWithAllocator<
                        CompiledTemplateProgramOwner>(
                        *self->allocator,
                        std::move(compiled).Value());
            if (!program) {
                return program.GetStatus();
            }
            CompiledTemplateBlueprint* programContext =
                &program.Value()->Blueprint();
            Base::Ref<Base::Object> programOwner =
                program.Value();
            Base::Result<void> configured;
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                configured = ::Aero::Controls::TemplatePrivate::Configure(dataTemplate,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = ::Aero::Controls::TemplatePrivate::Seal(dataTemplate);
                }
            } else {
                auto& itemsPanel =
                    static_cast<ItemsPanelTemplate&>(object);
                configured = ::Aero::Controls::TemplatePrivate::Configure(itemsPanel,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = ::Aero::Controls::TemplatePrivate::Seal(itemsPanel);
                }
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(dataTemplate);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(dataTemplate);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredNames(dataTemplate);
            } else {
                ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(
                    static_cast<ItemsPanelTemplate&>(object));
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        if (controlTemplate.GetTargetType() ==
            Meta::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                ::Aero::Controls::TemplatePrivate::SetTargetType(controlTemplate,
                    Control::StaticTypeId());
            if (!inferred) return inferred.GetStatus();
        }
        Base::Result<CompiledTemplateDefinition>
            compiled =
                CompileControlTemplateDefinition(
                    controlTemplate,
                    {
                        edges.Data(),
                        edges.Size()},
                    {
                        bindings.Data(),
                        bindings.Size()},
                    *self->runtime,
                    *self->properties);
        if (!compiled) {
            return compiled.GetStatus();
        }

        Base::Result<Base::Ref<CompiledTemplateProgramOwner>>
            program =
                Base::MakeRefWithAllocator<
                    CompiledTemplateProgramOwner>(
                    *self->allocator,
                    std::move(
                        compiled.Value().blueprint));
        if (!program) {
            return program.GetStatus();
        }
        CompiledTemplateBlueprint* programContext =
            &program.Value()->Blueprint();
        Base::Ref<Base::Object> programOwner =
            program.Value();

        Base::Result<void> configured =
            ::Aero::Controls::TemplatePrivate::ConfigureFactory(controlTemplate,
                &BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    ::Aero::Controls::TemplatePrivate::AddTemplateBinding(controlTemplate,
                            binding.targetName.View(),
                            binding.sourceProperty,
                            binding.targetProperty);
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (TemplatePropertyTrigger& trigger :
                 compiled.Value().propertyTriggers) {
                configured =
                    ::Aero::Controls::TemplatePrivate::AddPropertyTrigger(controlTemplate,
                        std::move(trigger));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (Controls::VisualStateGroupPlan& group :
                 compiled.Value().visualStateGroups) {
                configured =
                    ::Aero::Controls::TemplatePrivate::AddVisualStateGroup(controlTemplate,
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                ::Aero::Controls::TemplatePrivate::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualStateGroups(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredNames(controlTemplate);
        return {};
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr ||
            (scopeOwner.RuntimeType() !=
                 ControlTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 DataTemplate::StaticTypeId())) {
            return InvalidTemplateXaml(
                "Template name scope is invalid");
        }
        return scopeOwner.RuntimeType() ==
                ControlTemplate::StaticTypeId()
            ? ::Aero::Controls::TemplatePrivate::RegisterAuthoredName(
                  static_cast<ControlTemplate&>(scopeOwner), name, object)
            : ::Aero::Controls::TemplatePrivate::RegisterAuthoredName(
                  static_cast<DataTemplate&>(scopeOwner), name, object);
    }
};

static_assert(
    sizeof(XamlTemplateSchemaFacetState) <= 1024,
    "XamlTemplateSchemaFacet inline state storage is too small");
static_assert(
    alignof(XamlTemplateSchemaFacetState) <= alignof(std::max_align_t),
    "XamlTemplateSchemaFacet inline state alignment is insufficient");

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) XamlTemplateSchemaFacetState(
        runtime,
        properties,
        *allocator_);
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (state_ == nullptr) {
        return;
    }
    state_->~XamlTemplateSchemaFacetState();
    state_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        state_->schema != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML template extension registration is invalid");
    }
    const PropertyInfo* targetType =
        schema.Types().FindProperty(
            ControlTemplate::StaticTypeId(),
            Base::StringView("TargetType"),
            false);
    const PropertyInfo* targetName =
        schema.Types().FindProperty(
            Setter::StaticTypeId(),
            Base::StringView("TargetName"),
            false);
    const PropertyInfo* dataType =
        schema.Types().FindProperty(
            DataTemplate::StaticTypeId(),
            Base::StringView("DataType"),
            false);
    if (targetType == nullptr ||
        dataType == nullptr ||
        targetName == nullptr ||
        targetType->ValueType() !=
            TypeOf<TypeReference>() ||
        dataType->ValueType() !=
            TypeOf<TypeReference>() ||
        targetName->ValueType() !=
            TypeOf<Base::String>()) {
        return InvalidTemplateXaml(
            "Template XAML metadata is incomplete");
    }

    state_->schema = &schema;
    Base::Result<void> status =
        SchemaPrivate::AddType(schema, {
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacetState::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = SchemaPrivate::AddType(schema, {
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacetState::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = SchemaPrivate::AddType(schema, {
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        state_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


// ===== TemplateCompiler =====


#include "gui/controls/DataTemplateTriggerState.hpp"
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

#include <Aero/Controls.hpp>

#include "gui/controls/ControlBehavior.hpp"


namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace Aero::Media;


Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Object* ResolveTemplateBindingAncestor(
    const TemplatePrototypeBinding& binding,
    Base::Object& target,
    Meta::Registry& runtime) noexcept {
    if (binding.relativeAncestorType.Empty() ||
        binding.relativeAncestorLevel == 0U ||
        !runtime.Types().IsDerivedFrom(
            target.RuntimeType(), ::Aero::Media::Visual::StaticTypeId())) {
        return nullptr;
    }
    Base::StringView ancestorName =
        binding.relativeAncestorType.View();
    for (std::uint32_t index = 0U;
         index < ancestorName.SizeBytes(); ++index) {
        if (ancestorName[index] != ':') continue;
        ancestorName = ancestorName.Substr(
            index + 1U,
            ancestorName.SizeBytes() - index - 1U);
        break;
    }
    std::uint32_t matched = 0U;
    auto* targetVisual = static_cast<::Aero::Media::Visual*>(&target);
    ::Aero::Media::Visual* current = targetVisual->GetVisualParent();
    if (current == nullptr) current = targetVisual->GetLogicalParent();
    while (current != nullptr) {
        const TypeInfo* type =
            runtime.Types().FindType(current->RuntimeType());
        if ((ancestorName.Empty() ||
             (type != nullptr && type->Name() == ancestorName)) &&
            ++matched == binding.relativeAncestorLevel) {
            return current;
        }
        ::Aero::Media::Visual* next = current->GetVisualParent();
        if (next == nullptr) next = current->GetLogicalParent();
        current = next;
    }
    return nullptr;
}

struct PendingPrototypeNode {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    MemberId contentMember = InvalidMemberId;
};

bool RequiresPrototypeObject(
    Base::Span<const DeferredBindingEdge> bindings,
    const Base::Object* object) noexcept {
    for (const DeferredBindingEdge& binding : bindings) {
        if (binding.target == object ||
            binding.source == object) {
            return true;
        }
    }
    return false;
}

Base::Result<Value> ConvertTemplateTextValue(
    Meta::Registry& runtime,
    const DependencyProperty& property,
    Base::StringView text) noexcept {
    // WPF uses -1 for no selection. Internally selection keeps UINT32_MAX;
    // translate only the standard selection properties at the XAML boundary.
    if ((property.Handle() == Controls::Primitives::Selector::SelectedIndexProperty.Handle() ||
         property.Handle() == TabControl::SelectedIndexProperty.Handle()) &&
        text == Base::StringView("-1")) {
        return ValueCodec<std::uint32_t>::Encode(UINT32_MAX);
    }
    return runtime.TryConvertText(property.ValueType(), text);
}

const DependencyProperty* ResolveTemplateProperty(
    const DependencyPropertyRegistry& properties,
    TypeId targetType,
    Base::StringView name) noexcept {
    const DependencyProperty* property = properties.Find(targetType, name);
    if (property != nullptr) return property;
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX || separator == 0U ||
        separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }
    const Base::StringView ownerName = name.Substr(0U, separator);
    const Base::StringView memberName = name.Substr(
        separator + 1U, name.SizeBytes() - separator - 1U);
    const TypeInfo* owner = properties.Types().FindType(
        AeroNamespaceUri(), ownerName);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), memberName);
        if (property != nullptr) return property;
    }
    return properties.Find(targetType, memberName);
}

std::uint32_t FindPrototypeObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (std::uint32_t index = 0U;
         index < pending.Size();
         ++index) {
        if (pending[index].object.Get() == object) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<CompiledTemplateBlueprint>
CompileBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Base::Span<const Controls::TemplateMetadataBindingPlan>
        metadataBindings,
    Base::Span<const Controls::TemplateDynamicResourcePlan>
        dynamicResources,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!visualTree) {
        return InvalidTemplateCompiler(
            "Template requires a VisualTree");
    }

    CompiledTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;
    blueprint.properties = &properties;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended =
        pending.PushBack({
            visualTree,
            UINT32_MAX,
            InvalidMemberId});
    if (!appended) return appended.GetStatus();

    // Templated-parent bindings may target Freezables and other named
    // DependencyObjects nested in a visual property (for example a
    // GradientStop.Color binding). They are not visual edges, so retain them
    // explicitly in the prototype graph and later expose them through the
    // template instance name table.
    for (const Controls::TemplateMetadataBindingPlan& binding :
         metadataBindings) {
        if (binding.targetName.Empty() || names == nullptr) continue;
        Base::Object* target = names->Find(binding.targetName.View());
        if (target == nullptr ||
            runtime.Types().IsDerivedFrom(
                target->RuntimeType(), ::Aero::Media::Visual::StaticTypeId())) {
            continue;
        }
        if (!runtime.Types().IsDerivedFrom(
                target->RuntimeType(), DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "TemplatedParent Binding target is not a DependencyObject");
        }
        if (FindPrototypeObject(pending, target) != UINT32_MAX) continue;
        appended = pending.PushBack({
            Base::Ref<Base::Object>::FromBorrowed(*target),
            UINT32_MAX,
            InvalidMemberId});
        if (!appended) return appended.GetStatus();
    }
    for (const Controls::TemplateDynamicResourcePlan& resource :
         dynamicResources) {
        if (resource.targetName.Empty() || names == nullptr) continue;
        Base::Object* target = names->Find(resource.targetName.View());
        if (target == nullptr || runtime.Types().IsDerivedFrom(
                target->RuntimeType(), ::Aero::Media::Visual::StaticTypeId())) {
            continue;
        }
        if (!runtime.Types().IsDerivedFrom(
                target->RuntimeType(), DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "DynamicResource target is not a DependencyObject");
        }
        if (FindPrototypeObject(pending, target) != UINT32_MAX) continue;
        appended = pending.PushBack({
            Base::Ref<Base::Object>::FromBorrowed(*target),
            UINT32_MAX,
            InvalidMemberId});
        if (!appended) return appended.GetStatus();
    }

    for (std::uint32_t index = 0U;
         index < pending.Size(); ++index) {
        const PendingPrototypeNode& source =
            pending[index];
        Base::Object* object = source.object.Get();
        if (object == nullptr ||
            !runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Template graph contains a non-dependency object");
        }
        const bool visual = runtime.Types().IsDerivedFrom(
            object->RuntimeType(), ::Aero::Media::Visual::StaticTypeId());
        if (index == 0U && !visual) {
            return InvalidTemplateCompiler(
                "Template VisualTree root must be a Visual object");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentMember = source.contentMember;
        const Base::StringView name = names != nullptr
            ? names->NameOf(*object)
            : Base::StringView{};
        Base::Result<void> named =
            node.name.Assign(name);
        if (!named) return named.GetStatus();

        auto& dependencyObject =
            static_cast<DependencyObject&>(*object);
        for (const DependencyProperty& property :
             properties.Properties()) {
            if (property.MetadataFor(node.type) == nullptr) {
                continue;
            }
            Base::Result<Value> local =
                dependencyObject.ReadLocalValue(
                    property.Handle());
            if (!local) return local.GetStatus();
            if (local.Value().IsUnset()) continue;
            TemplatePrototypeProperty prototypeProperty;
            prototypeProperty.property = property.Handle();
            prototypeProperty.value = local.Value();
            if (local.Value().Kind() == ValueKind::Object &&
                !local.Value().IsNullObject() &&
                local.Value().AsObject() &&
                runtime.Types().IsDerivedFrom(
                    local.Value().AsObject()->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                prototypeProperty.objectNode = FindPrototypeObject(
                    pending, local.Value().AsObject().Get());
                const bool requiresClone =
                    prototypeProperty.objectNode != UINT32_MAX ||
                    RequiresPrototypeObject(
                        bindings, local.Value().AsObject().Get());
                if (requiresClone &&
                    prototypeProperty.objectNode == UINT32_MAX) {
                    prototypeProperty.objectNode = pending.Size();
                    appended = pending.PushBack({
                        local.Value().AsObject(),
                        UINT32_MAX,
                        InvalidMemberId});
                    if (!appended) return appended.GetStatus();
                }
            }
            appended = node.properties.PushBack(
                std::move(prototypeProperty));
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                Grid::StaticTypeId())) {
            const auto& grid =
                static_cast<const Grid&>(*object);
            appended = node.gridColumns.Assign(
                grid.GetColumnDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
            appended = node.gridRows.Assign(
                grid.GetRowDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
        }

        const bool contentPresenterCandidate =
            runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                node.type,
                ScrollContentPresenter::StaticTypeId());
        if (contentPresenterCandidate) {
            constexpr Base::StringView ScrollPresenterPart(
                "PART_ScrollContentPresenter");
            const bool explicitScrollPresenter =
                node.name.View() == ScrollPresenterPart;
            const bool exactScrollPresenter =
                node.type == ScrollContentPresenter::StaticTypeId();
            bool replace = blueprint.contentPresenter == UINT32_MAX;
            if (!replace &&
                blueprint.contentPresenter < blueprint.nodes.Size()) {
                const TemplatePrototypeNode& selected =
                    blueprint.nodes[blueprint.contentPresenter];
                const bool selectedExplicit =
                    selected.name.View() == ScrollPresenterPart;
                const bool selectedExact =
                    selected.type == ScrollContentPresenter::StaticTypeId();
                replace = explicitScrollPresenter ||
                    (!selectedExplicit && exactScrollPresenter &&
                     !selectedExact);
            }
            if (replace) {
                blueprint.contentPresenter = index;
            }
        }

        appended = blueprint.nodes.PushBack(
            std::move(node));
        if (!appended) return appended.GetStatus();

        for (const DeferredContentEdge& edge :
             edges) {
            if (edge.parent != object) continue;
            if (!edge.child) {
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a null child");
            }
            const std::uint32_t existing =
                FindPrototypeObject(pending, edge.child.Get());
            if (existing != UINT32_MAX) {
                if (existing == index) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate visual content contains a cycle");
                }
                PendingPrototypeNode& existingNode = pending[existing];
                if (existingNode.parent == UINT32_MAX) {
                    // A dependency object can first enter the prototype graph
                    // because a Binding or object-valued property references
                    // it, and later appear on its actual visual-content edge.
                    // Retain one clone and attach that clone at the authored
                    // location instead of treating the two discovery paths as
                    // duplicate visual content.
                    existingNode.parent = index;
                    existingNode.contentMember = edge.member;
                    continue;
                }
                if (existingNode.parent == index &&
                    existingNode.contentMember == edge.member) {
                    // Structural and content facets can report the same edge.
                    // It is one authored child, not two template instances.
                    continue;
                }
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a duplicate child");
            }
            appended = pending.PushBack({
                edge.child,
                index,
                edge.member});
            if (!appended) {
                return appended.GetStatus();
            }
        }
    }

    for (const DeferredBindingEdge& source :
         bindings) {
        const std::uint32_t target =
            FindPrototypeObject(
                pending, source.target);
        Base::Object* resolvedSource = source.source;
        if (resolvedSource == nullptr &&
            !source.sourceName.Empty() && names != nullptr) {
            resolvedSource = names->Find(source.sourceName.View());
        }
        const std::uint32_t bindingSource =
            resolvedSource != nullptr
            ? FindPrototypeObject(pending, resolvedSource)
            : UINT32_MAX;
        const bool externalElementName =
            resolvedSource == nullptr &&
            !source.sourceName.Empty();
        if (target == UINT32_MAX ||
            (source.source != nullptr && bindingSource == UINT32_MAX) ||
            source.metadata == nullptr) {
            return InvalidTemplateCompiler(
                "Deferred template Binding target or source is outside its VisualTree");
        }
        TemplatePrototypeBinding binding;
        binding.target = target;
        binding.source = bindingSource;
        binding.metadata = source.metadata;
        binding.targetProperty =
            source.targetProperty;
        binding.dataContextProperty =
            source.dataContextProperty;
        binding.bindsToSource =
            source.bindsToSource;
        binding.mode = source.mode;
        binding.updateSourceTrigger =
            source.updateSourceTrigger;
        if (externalElementName) {
            Base::Result<void> sourceName =
                binding.sourceName.Assign(
                    source.sourceName.View());
            if (!sourceName) return sourceName.GetStatus();
        }
        Base::Result<void> assigned =
            binding.relativeAncestorType.Assign(
            source.relativeAncestorType.View());
        if (!assigned) return assigned.GetStatus();
        binding.relativeAncestorLevel =
            source.relativeAncestorLevel;
        assigned = binding.path.Assign(
                source.path.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = binding.stringFormat.Assign(
            source.stringFormat.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = blueprint.bindings.PushBack(
            std::move(binding));
        if (!assigned) {
            return assigned.GetStatus();
        }
    }

    return blueprint;
}

const TemplatePrototypeNode* FindNode(
    const CompiledTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (const TemplatePrototypeNode& node :
         blueprint.nodes) {
        if (node.name.View() == name) {
            return &node;
        }
    }
    return nullptr;
}

Base::Result<Value> ConvertSetterValue(
    const Setter& setter,
    const TemplatePrototypeNode& target,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Value value = setter.GetAuthoredValue();
    if (value.IsUnset()) {
        return InvalidTemplateCompiler(
            "Visual state Setter requires Value");
    }
    // Bindings in template setters are declarations. Their source is only
    // available after the template has an instance and a templated parent.
    if (value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Data::Binding::StaticTypeId()) {
        return value;
    }
    if (property.AcceptsAnyValue()) {
        return value;
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            ConvertTemplateTextValue(
                runtime, property, value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    } else if (value.IsNullObject() &&
        property.ValueType() == TypeOf<Nullable<bool>>()) {
        Base::Result<Value> nullable =
            ValueCodec<Nullable<bool>>::Encode(
                Nullable<bool>{});
        if (!nullable) return nullable.GetStatus();
        value = std::move(nullable).Value();
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(
            property.ValueType());
    }
    if (property.ValueType() == Length::StaticTypeId() &&
        value.Type() == TypeOf<double>()) {
        Base::Result<double> numeric = ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        Base::Result<Value> length = ValueCodec<Length>::Encode(
            Length::Pixels(numeric.Value()));
        if (!length) return length.GetStatus();
        value = std::move(length).Value();
    }
    if (property.ValueType() ==
            Brush::StaticTypeId() &&
        value.Type() ==
            TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            ValueCodec<Base::Color>::Decode(
                value);
        if (!color) return color.GetStatus();
        Base::Result<Base::Ref<Brush>> brush =
            MakeSolidColorBrush(color.Value());
        if (!brush) return brush.GetStatus();
        value = Value::FromObject(
            Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    // The legacy editor backend stores text colors as Color values, while
    // WPF themes expose Foreground/Caret/Selection values as brushes. Accept
    // the common SolidColorBrush case at the markup boundary without
    // changing the authored resource or template semantics.
    if (property.ValueType() == TypeOf<Base::Color>() &&
        value.Kind() == ValueKind::Object &&
        runtime.Types().IsDerivedFrom(
            value.Type(), Brush::StaticTypeId())) {
        Base::Result<Base::Ref<Brush>> decoded =
            ValueCodec<Base::Ref<Brush>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        Brush* brush = decoded.Value().Get();
        if (brush == nullptr || brush->RuntimeType() !=
                SolidColorBrush::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "Color-backed text property requires SolidColorBrush");
        }
        Base::Result<Value> color = ValueCodec<Base::Color>::Encode(
            static_cast<SolidColorBrush*>(brush)->GetColor());
        if (!color) return color.GetStatus();
        value = std::move(color).Value();
    }

    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        thread_local char message[256];
        const TypeInfo* expected = runtime.Types().FindType(
            property.ValueType());
        const TypeInfo* actual = runtime.Types().FindType(value.Type());
        std::snprintf(
            message, sizeof(message),
            "Setter '%.*s' expects '%.*s' but received '%.*s'",
            static_cast<int>(property.Name().SizeBytes()), property.Name().Data(),
            expected != nullptr ? static_cast<int>(expected->Name().SizeBytes()) : 9,
            expected != nullptr ? expected->Name().Data() : "<unknown>",
            actual != nullptr ? static_cast<int>(actual->Name().SizeBytes()) : 9,
            actual != nullptr ? actual->Name().Data() : "<unknown>");
        return InvalidTemplateCompiler(message);
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(
            property.Handle(),
            target.type,
            value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
}

Base::Status MissingTemplateProperty(
    const char* role,
    Base::StringView property,
    TypeId targetType,
    const TypeRegistry& types) noexcept {
    thread_local char message[384];
    const TypeInfo* target =
        types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr
        ? target->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "ControlTemplate %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return InvalidTemplateCompiler(message);
}

Base::Result<Value> ConvertTriggerValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (value.IsUnset()) {
        return InvalidTemplateCompiler(
            "Template Trigger requires Value");
    }
    if (property.AcceptsAnyValue()) {
        return value;
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            ConvertTemplateTextValue(
                runtime, property, value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    } else if (value.IsNullObject() &&
        property.ValueType() == TypeOf<Nullable<bool>>()) {
        Base::Result<Value> nullable =
            ValueCodec<Nullable<bool>>::Encode(
                Nullable<bool>{});
        if (!nullable) return nullable.GetStatus();
        value = std::move(nullable).Value();
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(
            property.ValueType());
    }
    if (property.ValueType() == Length::StaticTypeId() &&
        value.Type() == TypeOf<double>()) {
        Base::Result<double> numeric = ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        Base::Result<Value> length = ValueCodec<Length>::Encode(
            Length::Pixels(numeric.Value()));
        if (!length) return length.GetStatus();
        value = std::move(length).Value();
    }
    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        return InvalidTemplateCompiler(
            "Template Trigger value type does not match its property");
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(
            property.Handle(), targetType, value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
}

Base::Result<Base::Vector<TemplatePropertyTrigger>>
CompilePropertyTriggers(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<TemplatePropertyTrigger> compiled;
    auto addCondition = [&controlTemplate, &blueprint, &runtime, &properties](
        Base::StringView propertyName,
        Base::StringView sourceName,
        const Value& authoredValue,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        if (propertyName.Empty() || authoredValue.IsUnset()) {
            return InvalidTemplateCompiler(
                "ControlTemplate Trigger condition requires Property and Value");
        }
        TypeId targetType = controlTemplate.GetTargetType();
        if (!sourceName.Empty()) {
            const TemplatePrototypeNode* target = FindNode(blueprint, sourceName);
            if (target == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger source was not found");
            }
            targetType = target->type;
        }
        const DependencyProperty* property =
            (propertyName == Base::StringView(
                "local:Element.IsFocusEngaged") ||
             propertyName == Base::StringView(
                "aero:Element.IsFocusEngaged"))
            ? properties.Find(
                Aero::Element::
                    IsFocusEngagedProperty.Handle())
            : propertyName == Base::StringView(
                "local:Text.PasswordLength")
            ? properties.Find(
                Aero::TextProperties::
                    PasswordLengthProperty.Handle())
            : ResolveTemplateProperty(properties, targetType, propertyName);
        if (property == nullptr) {
            return MissingTemplateProperty(
                "Trigger property", propertyName, targetType, runtime.Types());
        }
        Base::Result<Value> value = ConvertTriggerValue(
            authoredValue, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        TemplateTriggerCondition condition;
        Base::Result<void> assigned =
            condition.sourceName.Assign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        return trigger.conditions.PushBack(std::move(condition));
    };
    auto appendSetters = [&controlTemplate, &blueprint, &runtime, &properties](
        Base::Span<const Base::Ref<Setter>> setters,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject || setterObject->GetPropertyName().Empty()) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            const Base::StringView targetName = setterObject->GetTargetName();
            TypeId targetType = controlTemplate.GetTargetType();
            const TemplatePrototypeNode* target = nullptr;
            if (!targetName.Empty()) {
                target = FindNode(blueprint, targetName);
                if (target == nullptr) {
                    // RowDefinition/ColumnDefinition and similar named
                    // declaration objects do not belong to the visual clone
                    // graph. They remain represented by the owning Grid's
                    // serialized definition vectors; their trigger mutation
                    // is deferred until that declaration-object runtime is
                    // materialized.
                    if (::Aero::Controls::TemplatePrivate::AuthoredNames(controlTemplate).Find(
                            targetName) != nullptr) {
                        continue;
                    }
                    return InvalidTemplateCompiler(
                        "ControlTemplate Trigger Setter target was not found");
                }
                targetType = target->type;
            }
            const DependencyProperty* property = ResolveTemplateProperty(
                properties, targetType, setterObject->GetPropertyName());
            if (property == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter property was not found");
            }
            TemplatePrototypeNode parentTarget;
            parentTarget.type = targetType;
            Base::Result<Value> value = ConvertSetterValue(
                *setterObject,
                target != nullptr ? *target : parentTarget,
                *property, runtime, properties);
            if (!value) return value.GetStatus();
            TemplateTriggerSetter setter;
            Base::Result<void> assigned = setter.targetName.Assign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
            assigned = trigger.setters.PushBack(std::move(setter));
            if (!assigned) return assigned.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         ::Aero::Controls::TemplatePrivate::AuthoredTriggers(controlTemplate)) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            configured = addCondition(source.GetPropertyName(), source.GetSourceName(),
                source.GetAuthoredValue(), trigger);
            if (configured) configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else if (object && object->RuntimeType() == MultiTrigger::StaticTypeId()) {
            const auto& source = static_cast<const MultiTrigger&>(*object);
            for (const Base::Ref<Condition>& condition : source.GetConditions()) {
                if (!condition) return InvalidTemplateCompiler(
                    "MultiTrigger contains a null Condition");
                configured = addCondition(condition->GetPropertyName(), condition->GetSourceName(),
                    condition->GetAuthoredValue(), trigger);
                if (!configured) return configured.GetStatus();
            }
            configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else {
            // Data and event triggers are compiled into the instance runtime
            // plan below. They cannot be represented by the dependency-
            // property trigger table used by TemplateEngine.
            continue;
        }
        if (!configured) return configured.GetStatus();
        Base::Result<void> added =
            compiled.PushBack(
                std::move(trigger));
        if (!added) return added.GetStatus();
    }
    return compiled;
}

Base::Result<Base::Vector<Controls::VisualStateGroupPlan>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<Controls::VisualStateGroupPlan> groups;
    auto compileGroup = [&groups, &blueprint, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() !=
                VisualStateGroup::
                    StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<
                const VisualStateGroup&>(
                    *groupObject);
        if (sourceGroup.GetName().Empty()) {
            return InvalidTemplateCompiler(
                "VisualStateGroup requires Name");
        }
        Controls::VisualStateGroupPlan group;
        Base::Result<void> assigned =
            group.name.Assign(
                sourceGroup.GetName());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<VisualState>& stateObject :
             sourceGroup.GetStates()) {
            if (!stateObject ||
                stateObject->RuntimeType() !=
                    VisualState::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<
                const VisualState&>(
                    *stateObject);
            if (sourceState.GetName().Empty()) {
                return InvalidTemplateCompiler(
                    "VisualState requires Name");
            }
            Controls::VisualStatePlan state;
            assigned = state.name.Assign(
                sourceState.GetName());
            if (!assigned) {
                return assigned.GetStatus();
            }
            state.storyboard =
                sourceState.GetStoryboard();

            for (const Base::Ref<Base::Object>&
                     setterObject :
                 sourceState.GetSetters()) {
                if (!setterObject ||
                    setterObject->RuntimeType() !=
                        Setter::StaticTypeId()) {
                    return InvalidTemplateCompiler(
                        "VisualState contains an invalid Setter");
                }
                const auto& sourceSetter =
                    static_cast<const Setter&>(
                        *setterObject);
                if (sourceSetter.GetTargetName().Empty() ||
                    sourceSetter.GetPropertyName().Empty()) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter requires TargetName and Property");
                }
                const TemplatePrototypeNode* target =
                    FindNode(
                        blueprint,
                        sourceSetter.GetTargetName());
                if (target == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter target was not found");
                }
                const DependencyProperty* property =
                    ResolveTemplateProperty(
                        properties, target->type,
                        sourceSetter.GetPropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter property was not found");
                }
                Base::Result<Value> value =
                    ConvertSetterValue(
                        sourceSetter,
                        *target,
                        *property,
                        runtime,
                        properties);
                if (!value) {
                    return value.GetStatus();
                }

                Controls::VisualStateSetterPlan setter;
                assigned =
                    setter.targetName.Assign(
                        sourceSetter.GetTargetName());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                setter.property = property->Handle();
                setter.value =
                    std::move(value).Value();
                assigned =
                    state.setters.PushBack(
                        std::move(setter));
                if (!assigned) {
                    return assigned.GetStatus();
                }
            }
            assigned =
                group.states.PushBack(
                    std::move(state));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        for (const Base::Ref<VisualTransition>&
                  transitionObject :
              sourceGroup.GetTransitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() !=
                    VisualTransition::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition =
                static_cast<
                const VisualTransition&>(
                    *transitionObject);
            if (sourceTransition.GetFrom().Empty() &&
                sourceTransition.GetTo().Empty() &&
                sourceTransition.GetGeneratedDuration().Empty() &&
                !sourceTransition.GetStoryboard()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.GetFrom().Empty()) {
                bool found = false;
                for (const Controls::VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetFrom();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.GetTo().Empty()) {
                bool found = false;
                for (const Controls::VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetTo();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            Controls::VisualTransitionPlan transition;
            assigned = transition.from.Assign(
                sourceTransition.GetFrom());
            if (assigned) {
                assigned = transition.to.Assign(
                    sourceTransition.GetTo());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GetGeneratedDuration().Empty()) {
                Media::Animation::Storyboard duration;
                duration.SetDuration(sourceTransition.GetGeneratedDuration());
                transition.generatedDurationMicroseconds =
                    Aero::Media::AnimationPrivate::Timing(duration).durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GetGeneratedEasingFunction();
            transition.storyboard =
                sourceTransition.GetStoryboard();
            assigned = group.transitions.PushBack(
                std::move(transition));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = groups.PushBack(
            std::move(group));
        if (!assigned) return assigned.GetStatus();
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         ::Aero::Controls::TemplatePrivate::AuthoredVisualStateGroups(controlTemplate)) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        ::Aero::Controls::TemplatePrivate::AuthoredVisualTree(controlTemplate);
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            ::Aero::DependencyObject::StaticTypeId())) {
        auto& root = static_cast<::Aero::DependencyObject&>(*authoredRoot);
        Base::Ref<VisualStateGroupCollection> valueStore = root.GetValueOr(
            VisualStateManager::VisualStateGroupsProperty,
            Base::Ref<VisualStateGroupCollection>{});
        if (valueStore) {
            for (const Base::Ref<VisualStateGroup>& groupObject :
                 valueStore->GetItems()) {
                Base::Result<void> compiled = compileGroup(
                    Base::Ref<Base::Object>(groupObject));
                if (!compiled) return compiled.GetStatus();
            }
        }
    }
    return groups;
}

} // namespace

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<CompiledTemplateBlueprint> blueprint =
        CompileBlueprint(
        ::Aero::Controls::TemplatePrivate::AuthoredVisualTree(controlTemplate),
        &::Aero::Controls::TemplatePrivate::AuthoredNames(controlTemplate),
        edges,
        bindings,
        ::Aero::Controls::TemplatePrivate::MetadataBindings(
            controlTemplate),
        ::Aero::Controls::TemplatePrivate::DynamicResources(
            controlTemplate),
        runtime,
        properties);
    if (!blueprint) {
        return blueprint.GetStatus();
    }
    Base::Result<Base::Vector<Controls::VisualStateGroupPlan>>
        groups = CompileVisualStates(
            controlTemplate,
            blueprint.Value(),
            runtime,
            properties);
    if (!groups) return groups.GetStatus();
    Base::Result<Base::Vector<TemplatePropertyTrigger>>
        triggers = CompilePropertyTriggers(
            controlTemplate,
            blueprint.Value(),
            runtime,
            properties);
    if (!triggers) return triggers.GetStatus();

    for (const Base::Ref<Base::Object>& authored :
         ::Aero::Controls::TemplatePrivate::AuthoredTriggers(controlTemplate)) {
        if (!authored) continue;
        if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
            authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
            (authored->RuntimeType() == Trigger::StaticTypeId() &&
             (!static_cast<const Trigger&>(*authored).
                    GetEnterActions().Empty() ||
              !static_cast<const Trigger&>(*authored).
                    GetExitActions().Empty()))) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate instance trigger cannot be retained");
            }
            Base::Result<void> added =
                blueprint.Value().controlTemplateDataTriggers.PushBack(
                    std::move(retained));
            if (!added) return added.GetStatus();
        } else if (authored->RuntimeType() ==
                   Media::Animation::EventTrigger::StaticTypeId()) {
            Base::Ref<Media::Animation::EventTrigger> retained =
                Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
                    static_cast<Media::Animation::EventTrigger&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate EventTrigger cannot be retained");
            }
            Base::Result<void> added =
                blueprint.Value().controlTemplateEventTriggers.PushBack(
                    std::move(retained));
            if (!added) return added.GetStatus();
        }
    }

    CompiledTemplateDefinition definition;
    definition.targetType =
        controlTemplate.GetTargetType();
    definition.blueprint =
        std::move(blueprint).Value();
    for (std::uint32_t nodeIndex = 0U;
         nodeIndex < definition.blueprint.nodes.Size(); ++nodeIndex) {
        TemplatePrototypeNode& node =
            definition.blueprint.nodes[nodeIndex];
        if (!runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId())) {
            continue;
        }
        const DependencyProperty* contentSourceProperty =
            properties.Find(
                ContentPresenter::
                    ContentSourceProperty.Handle());
        const DependencyProperty* contentProperty =
            properties.Find(
                ContentPresenter::
                    ContentProperty.Handle());
        if (contentSourceProperty == nullptr ||
            contentProperty == nullptr) {
            return InvalidTemplateCompiler(
                "ContentPresenter content properties were not registered");
        }
        Base::StringView contentSource;
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            if (property.property ==
                    contentSourceProperty->Handle() &&
                property.value.Kind() ==
                    ValueKind::String) {
                contentSource =
                    property.value.AsString();
                break;
            }
        }
        if (contentSource.Empty()) continue;
        if (node.name.Empty()) {
            char generatedName[48];
            const int written = std::snprintf(
                generatedName, sizeof(generatedName),
                "__ContentSource%u", nodeIndex);
            if (written <= 0 || static_cast<std::size_t>(written) >=
                                    sizeof(generatedName)) {
                return InvalidTemplateCompiler(
                    "ContentPresenter generated template name is invalid");
            }
            Base::Result<void> named = node.name.Assign(
                Base::StringView(
                    generatedName,
                    static_cast<std::uint32_t>(written)));
            if (!named) return named.GetStatus();
        }
        const DependencyProperty* source =
            properties.Find(
                controlTemplate.GetTargetType(),
                contentSource);
        if (source == nullptr) {
            return MissingTemplateProperty(
                "ContentSource property",
                contentSource,
                controlTemplate.GetTargetType(),
                runtime.Types());
        }
        TemplateBindingPlan binding;
        Base::Result<void> assigned =
            binding.targetName.Assign(
                node.name.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        binding.sourceProperty =
            source->Handle();
        binding.targetProperty =
            contentProperty->Handle();
        assigned =
            definition.contentSourceBindings.
                PushBack(std::move(binding));
        if (!assigned) {
            return assigned.GetStatus();
        }
    }
    definition.propertyTriggers =
        std::move(triggers).Value();
    definition.visualStateGroups =
        std::move(groups).Value();
    return definition;
}

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return CompileBlueprint(
        visualTree,
        names,
        edges,
        bindings,
        {},
        {},
        runtime,
        properties);
}

std::uint32_t FindNodeIndex(
    const CompiledTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (std::uint32_t index = 0U;
         index < blueprint.nodes.Size();
         ++index) {
        if (blueprint.nodes[index].name.View() ==
                name) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<bool> DeferredTriggerValuesMatch(
    const Value& actual,
    Value expected,
    Meta::Registry& runtime) noexcept {
    if (actual.Kind() == ValueKind::Object &&
        !actual.IsNullObject() &&
        actual.AsObject() &&
        actual.AsObject()->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        return DeferredTriggerValuesMatch(
            static_cast<const ::Aero::Controls::BoxedItemValue&>(
                *actual.AsObject()).Value(),
            std::move(expected),
            runtime);
    }
    if (expected.Kind() == ValueKind::String &&
        expected.Type() != actual.Type()) {
        Base::Result<Value> converted =
            runtime.TryConvertText(
                actual.Type(),
                expected.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        expected = std::move(converted).Value();
    }
    return actual == expected;
}

Base::Result<Value> ReadDeferredTriggerBinding(
    const Data::Binding& binding,
    const Base::Ref<Base::Object>& payload,
    Meta::Registry& runtime) noexcept {
    if (!payload || binding.GetPath().GetPath().Empty()) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger Binding requires a data item and Path");
    }
    if (!binding.GetElementName().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DataTemplate trigger ElementName source is outside the item scope");
    }
    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            runtime,
            payload->RuntimeType(),
            binding.GetPath().GetPath());
    if (!compiled) return compiled.GetStatus();
    return compiled.Value().Get(
        runtime, *payload);
}

Base::Result<void> ApplyDeferredTriggerSetters(
    const CompiledTemplateBlueprint& blueprint,
    Base::Span<const Base::Ref<Setter>> setters,
    Base::Vector<Base::Ref<Base::Object>>& objects) noexcept {
    if (blueprint.runtime == nullptr ||
        blueprint.properties == nullptr) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger services are unavailable");
    }
    for (const Base::Ref<Setter>& setter :
         setters) {
        if (!setter || !setter->GetIsAuthored()) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter is incomplete");
        }
        const std::uint32_t target =
            setter->GetTargetName().Empty()
            ? 0U
            : FindNodeIndex(
                  blueprint,
                  setter->GetTargetName());
        if (target >= objects.Size()) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter target was not found");
        }
        auto* dependencyObject =
            static_cast<DependencyObject*>(
                objects[target].Get());
        const DependencyProperty* property =
            blueprint.properties->Find(
                objects[target]->RuntimeType(),
                setter->GetPropertyName());
        if (property == nullptr) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter property was not found");
        }
        Base::Result<Value> converted =
            ConvertSetterValue(
                *setter,
                blueprint.nodes[target],
                *property,
                *blueprint.runtime,
                *blueprint.properties);
        if (!converted) return converted.GetStatus();
        dependencyObject->SetValue(property->Handle(),
            std::move(converted).Value());
    }
    return {};
}

Base::Result<void> ApplyInitialDataTemplateTriggers(
    const CompiledTemplateBlueprint& blueprint,
    const Base::Ref<Base::Object>& payload,
    Base::Vector<Base::Ref<Base::Object>>& objects) noexcept {
    if (blueprint.dataTemplateTriggers.Empty()) {
        return {};
    }
    if (blueprint.runtime == nullptr) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger metadata is unavailable");
    }
    for (const Base::Ref<TriggerBase>& trigger :
         blueprint.dataTemplateTriggers) {
        if (!trigger) continue;
        if (trigger->RuntimeType() ==
                DataTrigger::StaticTypeId()) {
            const auto& dataTrigger =
                static_cast<const DataTrigger&>(
                    *trigger);
            if (!dataTrigger.GetBinding()) {
                return InvalidTemplateCompiler(
                    "DataTrigger requires Binding");
            }
            Base::Result<Value> current =
                ReadDeferredTriggerBinding(
                    *dataTrigger.GetBinding(),
                    payload,
                    *blueprint.runtime);
            if (!current) {
                if (current.GetStatus().code ==
                        Base::ErrorCode::NotFound) {
                    continue;
                }
                return current.GetStatus();
            }
            Base::Result<bool> matches =
                DeferredTriggerValuesMatch(
                    current.Value(),
                    dataTrigger.GetAuthoredValue(),
                    *blueprint.runtime);
            if (!matches) return matches.GetStatus();
            if (matches.Value()) {
                Base::Result<void> applied =
                    ApplyDeferredTriggerSetters(
                        blueprint,
                        dataTrigger.GetAuthoredSetters(),
                        objects);
                if (!applied) return applied.GetStatus();
            }
        } else if (trigger->RuntimeType() ==
                   MultiDataTrigger::StaticTypeId()) {
            const auto& multi =
                static_cast<const MultiDataTrigger&>(
                    *trigger);
            bool active = true;
            for (const Base::Ref<Condition>& condition :
                 multi.GetConditions()) {
                if (!condition ||
                    !condition->GetBinding()) {
                    return InvalidTemplateCompiler(
                        "MultiDataTrigger requires complete Conditions");
                }
                Base::Result<Value> current =
                    ReadDeferredTriggerBinding(
                        *condition->GetBinding(),
                        payload,
                        *blueprint.runtime);
                if (!current &&
                    current.GetStatus().code ==
                        Base::ErrorCode::NotFound) {
                    active = false;
                    break;
                }
                if (!current) {
                    return current.GetStatus();
                }
                Base::Result<bool> matches =
                    DeferredTriggerValuesMatch(
                        current.Value(),
                        condition->GetAuthoredValue(),
                        *blueprint.runtime);
                if (!matches) {
                    return matches.GetStatus();
                }
                if (!matches.Value()) {
                    active = false;
                    break;
                }
            }
            if (active) {
                Base::Result<void> applied =
                    ApplyDeferredTriggerSetters(
                        blueprint,
                        multi.GetAuthoredSetters(),
                        objects);
                if (!applied) return applied.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void> BuildCompiledTemplate(
    TemplateBuilder& context,
    void* factoryContext) noexcept {
    auto* blueprint =
        static_cast<CompiledTemplateBlueprint*>(
            factoryContext);
    if (blueprint == nullptr ||
        blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTemplateCompiler(
            "Compiled template blueprint is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> reserved =
        objects.Reserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<::Aero::Media::Visual*> visuals;
    reserved = visuals.Reserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node =
            blueprint->nodes[index];
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(
                node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner =
            std::move(created).Value();
        if (!owner || owner->RuntimeType() != node.type ||
            !blueprint->runtime->Types().IsDerivedFrom(
                node.type, DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Compiled template factory created an incompatible object");
        }
        ::Aero::Media::Visual* visual = blueprint->runtime->Types().IsDerivedFrom(
            node.type, ::Aero::Media::Visual::StaticTypeId())
            ? static_cast<::Aero::Media::Visual*>(owner.Get()) : nullptr;
        Base::Result<void> added = objects.PushBack(std::move(owner));
        if (!added) return added.GetStatus();
        added = visuals.PushBack(visual);
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        auto& dependencyObject = static_cast<DependencyObject&>(*objects[index]);
        for (const TemplatePrototypeProperty& property : node.properties) {
            Value value = property.value;
            if (property.objectNode != UINT32_MAX) {
                if (property.objectNode >= objects.Size()) {
                    return InvalidTemplateCompiler(
                        "Compiled template property object is invalid");
                }
                value = Value::FromObject(
                    property.value.Type(), objects[property.objectNode]);
            }
            dependencyObject.SetValue(property.property, std::move(value));
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            grid.SetColumnDefinitions(node.gridColumns.AsSpan());
            grid.SetRowDefinitions(node.gridRows.AsSpan());
        }
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        if (node.parent != UINT32_MAX) {
            if (node.parent >= objects.Size() ||
                node.contentMember == InvalidMemberId) {
                return InvalidTemplateCompiler(
                    "Compiled template content edge is invalid");
            }
            const PropertyInfo* structuralProperty = blueprint->runtime->Types()
                .FindProperty(node.contentMember);
            const bool propertyEdge = structuralProperty != nullptr &&
                (static_cast<std::uint32_t>(structuralProperty->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Structural)) != 0U &&
                blueprint->runtime->CanWriteProperty(node.contentMember);
            Base::Result<void> content = propertyEdge
                ? blueprint->runtime->SetProperty(
                    *objects[node.parent], node.contentMember,
                    Value::FromObject(structuralProperty->ValueType(), objects[index]))
                : blueprint->runtime->WriteContent(
                    *objects[node.parent], node.contentMember, objects[index]);
            if (!content) return content.GetStatus();
        }
        if (visuals[index] == nullptr) {
            if (!node.name.Empty()) {
                Base::Result<void> named = context.AddObjectPart(
                    node.name.View(),
                    Base::Ref<Base::Object>::FromBorrowed(*objects[index]),
                    *static_cast<DependencyObject*>(objects[index].Get()));
                if (!named) return named.GetStatus();
            }
            continue;
        }
        if (node.parent != UINT32_MAX &&
            visuals[node.parent] == nullptr) {
            return InvalidTemplateCompiler(
                "::Aero::Media::Visual template child has a non-Visual parent");
        }
        Base::Result<void> added = node.parent == UINT32_MAX
            ? context.SetRoot(node.name.View(),
                Base::Ref<Base::Object>::FromBorrowed(*objects[index]), *visuals[index])
            : context.AddPart(node.name.View(), *visuals[node.parent],
                Base::Ref<Base::Object>::FromBorrowed(*objects[index]), *visuals[index]);
        if (!added) return added.GetStatus();
    }

    if (blueprint->contentPresenter != UINT32_MAX) {
        if (blueprint->contentPresenter >=
                visuals.Size()) {
            return InvalidTemplateCompiler(
                "Compiled ContentPresenter index is invalid");
        }
        if (blueprint->runtime->Types()
                .IsDerivedFrom(
                    context.TemplatedParent()
                        .RuntimeType(),
                    ContentControl::StaticTypeId())) {
            ::Aero::Media::Visual& contentHost =
                *visuals[
                    blueprint->contentPresenter];
            Base::Result<bool> projected =
                blueprint->runtime->Types().
                    IsDerivedFrom(
                        contentHost.RuntimeType(),
                        ContentPresenter::
                            StaticTypeId())
                ? context.ProjectContent(
                      static_cast<ContentControl&>(
                          context.
                              TemplatedParent()),
                      static_cast<ContentPresenter&>(
                          contentHost))
                : context.ProjectContent(
                      static_cast<ContentControl&>(
                          context.
                              TemplatedParent()),
                      static_cast<ContentControl&>(
                          contentHost));
            if (!projected) {
                return projected.GetStatus();
            }
        }
    }
    for (const TemplatePrototypeBinding& binding :
         blueprint->bindings) {
        if (binding.metadata == nullptr ||
            binding.target >= objects.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= objects.Size())) {
            return InvalidTemplateCompiler(
                "Compiled template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        if (descriptor.source == nullptr &&
            !binding.sourceName.Empty()) {
            descriptor.source = context.TemplatedParent().FindName(
                binding.sourceName.View());
            if (descriptor.source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "ControlTemplate Binding ElementName was not found in the outer NameScope");
            }
        }
        if (descriptor.source == nullptr &&
            !binding.relativeAncestorType.Empty()) {
            descriptor.source = ResolveTemplateBindingAncestor(
                binding,
                *objects[binding.target],
                *blueprint->runtime);
            if (descriptor.source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "ControlTemplate Binding ancestor was not found");
            }
        }
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.bindsToSource =
            binding.bindsToSource;
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<void> queued =
            context.Bindings().QueueDeferred(descriptor);
        if (!queued) return queued.GetStatus();
    }

    if (!blueprint->controlTemplateDataTriggers.Empty()) {
        if (!blueprint->runtime->Types().IsDerivedFrom(
                visuals[0U]->RuntimeType(),
                FrameworkElement::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "ControlTemplate root does not support runtime triggers");
        }
        Base::Result<Base::Ref<
            Aero::Controls::DataTemplateTriggerState>> created =
            Base::MakeRef<Aero::Controls::DataTemplateTriggerState>();
        if (!created) return created.GetStatus();
        Base::Ref<Aero::Controls::DataTemplateTriggerState> triggerContext =
            std::move(created).Value();
        triggerContext->root =
            static_cast<FrameworkElement*>(visuals[0U]);
        for (std::uint32_t index = 0U; index < visuals.Size(); ++index) {
            if (blueprint->nodes[index].name.Empty()) continue;
            Aero::Controls::DataTemplateTriggerState::NamedObject named;
            Base::Result<void> namedAssigned = named.name.Assign(
                blueprint->nodes[index].name.View());
            if (!namedAssigned) return namedAssigned.GetStatus();
            named.object = Base::WeakRef<Base::Object>(
                Base::Ref<Base::Object>::FromBorrowed(*objects[index]));
            namedAssigned = triggerContext->names.PushBack(
                std::move(named));
            if (!namedAssigned) return namedAssigned.GetStatus();
        }
        auto appendSetters =
            [&](Base::Span<const Base::Ref<Setter>> setters,
                Aero::Controls::DataTemplatePropertyTrigger& runtimeTrigger)
                noexcept -> Base::Result<void> {
            for (const Base::Ref<Setter>& setter : setters) {
                if (!setter) continue;
                const std::uint32_t target = setter->GetTargetName().Empty()
                    ? 0U : FindNodeIndex(*blueprint, setter->GetTargetName());
                if (target >= visuals.Size()) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate data trigger Setter target was not found");
                }
                const DependencyProperty* property = ResolveTemplateProperty(
                    *blueprint->properties, visuals[target]->RuntimeType(),
                    setter->GetPropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate data trigger Setter property was not found");
                }
                Base::Result<Value> value = ConvertSetterValue(
                    *setter, blueprint->nodes[target], *property,
                    *blueprint->runtime, *blueprint->properties);
                if (!value) return value.GetStatus();
                Aero::Controls::DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(visuals[target])));
                runtimeSetter.property = property->Handle();
                runtimeSetter.value = std::move(value).Value();
                Base::Result<void> added = runtimeTrigger.setters.PushBack(
                    std::move(runtimeSetter));
                if (!added) return added.GetStatus();
            }
            return {};
        };
        auto sourceFor = [&](const Data::Binding& binding) noexcept
            -> Base::Object* {
            if (!binding.GetElementName().Empty()) {
                return triggerContext->FindName(binding.GetElementName());
            }
            const Base::Ref<Data::RelativeSource> relative =
                binding.GetRelativeSource();
            if (!relative) {
                // The default source is not the object visible while the
                // template prototype is compiled. It is the templated
                // parent's current DataContext after the instance has been
                // attached and its inherited values have settled.
                return static_cast<Base::Object*>(&context.TemplatedParent());
            }
            if (relative && relative->GetMode() ==
                    Data::RelativeSourceMode::FindAncestor) {
                Base::StringView ancestorName = relative->GetAncestorType();
                for (std::uint32_t nameIndex = 0U;
                     nameIndex < ancestorName.SizeBytes(); ++nameIndex) {
                    if (ancestorName[nameIndex] == ':') {
                        ancestorName = ancestorName.Substr(
                            nameIndex + 1U,
                            ancestorName.SizeBytes() - nameIndex - 1U);
                        break;
                    }
                }
                std::uint32_t matchedLevel = 0U;
                ::Aero::Media::Visual* current =
                    context.TemplatedParent().GetLogicalParent();
                if (current == nullptr) {
                    current = context.TemplatedParent().GetVisualParent();
                }
                while (current != nullptr) {
                    const TypeInfo* type = blueprint->runtime->Types().FindType(
                        current->RuntimeType());
                    const bool matches = ancestorName.Empty() ||
                        (type != nullptr && type->Name() == ancestorName);
                    if (matches && ++matchedLevel ==
                            relative->GetAncestorLevel()) {
                        return static_cast<Base::Object*>(current);
                    }
                    ::Aero::Media::Visual* next = current->GetLogicalParent();
                    if (next == nullptr) next = current->GetVisualParent();
                    current = next;
                }
                return nullptr;
            }
            // Explicit Self and TemplatedParent resolve to the templated
            // control. The default case above resolves its DataContext.
            return static_cast<Base::Object*>(&context.TemplatedParent());
        };
        for (const Base::Ref<TriggerBase>& authored :
             blueprint->controlTemplateDataTriggers) {
            if (!authored) continue;
            Aero::Controls::DataTemplatePropertyTrigger runtimeTrigger;
            Base::Span<const Base::Ref<Setter>> setters;
            if (authored->RuntimeType() == Trigger::StaticTypeId()) {
                const auto& property =
                    static_cast<const Trigger&>(*authored);
                Base::Object* source = property.GetSourceName().Empty()
                    ? static_cast<Base::Object*>(&context.TemplatedParent())
                    : triggerContext->FindName(property.GetSourceName());
                if (source == nullptr ||
                    !blueprint->runtime->Types().IsDerivedFrom(
                        source->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate action trigger source was not found");
                }
                const TypeId sourceType = source->RuntimeType();
                const DependencyProperty* sourceProperty =
                    property.GetPropertyName() == Base::StringView(
                        "local:Element.IsFocusEngaged")
                    ? blueprint->properties->Find(
                        Aero::Element::
                            IsFocusEngagedProperty.Handle())
                    : blueprint->properties->Find(
                        sourceType, property.GetPropertyName());
                if (sourceProperty == nullptr) {
                    return MissingTemplateProperty(
                        "ControlTemplate action trigger property",
                        property.GetPropertyName(), sourceType,
                        blueprint->runtime->Types());
                }
                Aero::Controls::DataTemplateTriggerCondition condition;
                condition.source = Base::WeakRef<Base::Object>(
                    Base::Ref<Base::Object>::FromBorrowed(*source));
                condition.dependencySource =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(source)));
                condition.property = sourceProperty->Handle();
                Base::Result<Value> converted = ConvertTriggerValue(
                    property.GetAuthoredValue(), sourceType, *sourceProperty,
                    *blueprint->runtime, *blueprint->properties);
                if (!converted) return converted.GetStatus();
                condition.value = std::move(converted).Value();
                Base::Result<void> added = runtimeTrigger.conditions.PushBack(
                    std::move(condition));
                if (!added) return added.GetStatus();
                // Property-trigger setters continue to be owned by
                // TemplateEngine, which preserves their trigger precedence.
                // This per-instance plan supplies only the action lifecycle.
            } else if (authored->RuntimeType() == DataTrigger::StaticTypeId()) {
                const auto& data = static_cast<const DataTrigger&>(*authored);
                if (!data.GetBinding()) return InvalidTemplateCompiler(
                    "ControlTemplate DataTrigger requires Binding");
                Aero::Controls::DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*data.GetBinding());
                if (source != nullptr) {
                    condition.source =
                        Base::WeakRef<Base::Object>(
                            Base::Ref<Base::Object>::FromBorrowed(*source));
                }
                condition.usesDataContext =
                    data.GetBinding()->GetElementName().Empty() &&
                    !data.GetBinding()->GetRelativeSource();
                condition.binding = data.GetBinding();
                condition.value = data.GetAuthoredValue();
                Base::Result<void> added = runtimeTrigger.conditions.PushBack(
                    std::move(condition));
                if (!added) return added.GetStatus();
                setters = data.GetAuthoredSetters();
            } else if (authored->RuntimeType() ==
                       MultiDataTrigger::StaticTypeId()) {
                const auto& multi = static_cast<const MultiDataTrigger&>(*authored);
                for (const Base::Ref<Condition>& authoredCondition :
                     multi.GetConditions()) {
                    if (!authoredCondition || !authoredCondition->GetBinding()) {
                        return InvalidTemplateCompiler(
                            "ControlTemplate MultiDataTrigger requires complete Conditions");
                    }
                    Aero::Controls::DataTemplateTriggerCondition condition;
                    Base::Object* source = sourceFor(*authoredCondition->GetBinding());
                    if (source != nullptr) {
                        condition.source =
                            Base::WeakRef<Base::Object>(
                                Base::Ref<Base::Object>::FromBorrowed(*source));
                    }
                    condition.usesDataContext =
                        authoredCondition->GetBinding()->GetElementName().Empty() &&
                        !authoredCondition->GetBinding()->GetRelativeSource();
                    condition.binding = authoredCondition->GetBinding();
                    condition.value = authoredCondition->GetAuthoredValue();
                    Base::Result<void> added =
                        runtimeTrigger.conditions.PushBack(std::move(condition));
                    if (!added) return added.GetStatus();
                }
                setters = multi.GetAuthoredSetters();
            } else {
                continue;
            }
            Base::Result<void> configured = appendSetters(setters, runtimeTrigger);
            if (configured) {
                configured = runtimeTrigger.enterActions.Append(
                    authored->GetEnterActions());
            }
            if (configured) {
                configured = runtimeTrigger.exitActions.Append(
                    authored->GetExitActions());
            }
            if (configured) {
                configured = triggerContext->triggers.PushBack(
                    std::move(runtimeTrigger));
            }
            if (!configured) return configured.GetStatus();
        }
        // Do not move triggerContext in the same call expression that reads
        // triggerContext->root. C++17 does not order function arguments, so
        // the converting move could clear the Ref before the root argument is
        // evaluated. Keep the owner alive through both evaluations.
        FrameworkElement* const triggerRoot = triggerContext->root;
        Base::Ref<Base::Object> triggerOwner(triggerContext);
        Base::Result<void> attached =
            Aero::ElementPrivate::AddAuthoredTrigger(
                *triggerRoot, std::move(triggerOwner));
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    Aero::BindingEngine* bindings) noexcept {
    auto* blueprint =
        static_cast<CompiledTemplateBlueprint*>(
            factoryContext);
    if (blueprint == nullptr ||
        blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTemplateCompiler(
            "Deferred template blueprint is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> reserved =
        objects.Reserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();

    for (const TemplatePrototypeNode& node : blueprint->nodes) {
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner = std::move(created).Value();
        if (!owner || !blueprint->runtime->Types().IsDerivedFrom(
                          node.type, DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Deferred template created an incompatible object");
        }
        Base::Result<void> added = objects.PushBack(std::move(owner));
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        auto& dependencyObject =
            static_cast<DependencyObject&>(*objects[index]);
        for (const TemplatePrototypeProperty& property : node.properties) {
            Value value = property.value;
            if (property.objectNode != UINT32_MAX) {
                if (property.objectNode >= objects.Size()) {
                    return InvalidTemplateCompiler(
                        "Deferred template property object is invalid");
                }
                value = Value::FromObject(
                    property.value.Type(), objects[property.objectNode]);
            }
            dependencyObject.SetValue(property.property, std::move(value));
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            grid.SetColumnDefinitions(node.gridColumns.AsSpan());
            grid.SetRowDefinitions(node.gridRows.AsSpan());
        }
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        if (node.parent == UINT32_MAX) continue;
        if (node.parent >= objects.Size() ||
            node.contentMember == InvalidMemberId) {
            return InvalidTemplateCompiler(
                "Deferred template content edge is invalid");
        }
        const PropertyInfo* structuralProperty = blueprint->runtime->Types()
            .FindProperty(node.contentMember);
        const bool propertyEdge = structuralProperty != nullptr &&
            (static_cast<std::uint32_t>(structuralProperty->Flags()) &
             static_cast<std::uint32_t>(PropertyFlags::Structural)) != 0U &&
            blueprint->runtime->CanWriteProperty(node.contentMember);
        Base::Result<void> written = propertyEdge
            ? blueprint->runtime->SetProperty(
                *objects[node.parent], node.contentMember,
                Value::FromObject(structuralProperty->ValueType(), objects[index]))
            : blueprint->runtime->WriteContent(
                *objects[node.parent], node.contentMember, objects[index]);
        if (!written) return written.GetStatus();
    }

    Base::Ref<Base::Object> root = objects[0U];
    if (payload &&
        blueprint->runtime->Types().IsDerivedFrom(
            root->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        static_cast<FrameworkElement&>(*root).SetDataContext(payload);
    }
    for (const TemplatePrototypeBinding& binding :
         blueprint->bindings) {
        if (binding.metadata == nullptr ||
            binding.target >= objects.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= objects.Size())) {
            return InvalidTemplateCompiler(
                "Deferred template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        if (descriptor.source == nullptr &&
            !binding.sourceName.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "DataTemplate Binding cannot resolve an ElementName outside its template NameScope");
        }
        if (descriptor.source == nullptr &&
            !binding.relativeAncestorType.Empty()) {
            descriptor.source = ResolveTemplateBindingAncestor(
                binding,
                *objects[binding.target],
                *blueprint->runtime);
            if (descriptor.source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "DataTemplate Binding ancestor was not found");
            }
        }
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.bindsToSource =
            binding.bindsToSource;
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        if (bindings == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Binding requires a mounted View binding engine");
        }
        Base::Result<void> queued =
            bindings->QueueDeferred(descriptor);
        if (!queued) return queued.GetStatus();
        // A DataTemplate receives its item as the DataContext of its root
        // before it is attached to an ItemsHost. Activate bindings targeted
        // at that root now: closed selectors and other content projections
        // may consume their template presentation before a popup is visible.
        // Nested elements remain deferred until the normal visual-tree walk
        // establishes inherited values for them.
        if (payload &&
            binding.source == UINT32_MAX &&
            descriptor.target == root.Get()) {
            Base::Result<void> activated =
                bindings->ActivateDeferredWhenReady(*descriptor.target);
            if (!activated) return activated.GetStatus();
        }
    }
    Base::Ref<Aero::Controls::DataTemplateTriggerState>
        triggerContext;
    auto ensureTriggerContext =
        [&]() noexcept
        -> Base::Result<
            Aero::Controls::DataTemplateTriggerState*> {
        if (triggerContext) {
            return triggerContext.Get();
        }
        Base::Result<Base::Ref<
            Aero::Controls::DataTemplateTriggerState>>
            created = Base::MakeRef<
                Aero::Controls::DataTemplateTriggerState>();
        if (!created) {
            return created.GetStatus();
        }
        triggerContext = std::move(created).Value();
        triggerContext->root =
            static_cast<FrameworkElement*>(root.Get());
        for (std::uint32_t index = 0U;
             index < objects.Size();
             ++index) {
            if (blueprint->nodes[index].name.Empty()) {
                continue;
            }
            Aero::Controls::DataTemplateTriggerState::
                NamedObject named;
            Base::Result<void> assigned =
                named.name.Assign(
                    blueprint->nodes[index].name.View());
            if (!assigned) {
                return assigned.GetStatus();
            }
            named.object = Base::WeakRef<Base::Object>(objects[index]);
            assigned =
                triggerContext->names.PushBack(
                    std::move(named));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        return triggerContext.Get();
    };
    auto appendRuntimeSetters =
        [&](Base::Span<const Base::Ref<Setter>> setters,
            Aero::Controls::DataTemplatePropertyTrigger&
                runtimeTrigger) noexcept
        -> Base::Result<void> {
        for (const Base::Ref<Setter>& setter : setters) {
            if (!setter) continue;
            const std::uint32_t targetIndex =
                setter->GetTargetName().Empty()
                ? 0U
                : FindNodeIndex(
                      *blueprint,
                      setter->GetTargetName());
            if (targetIndex >= objects.Size()) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger Setter target was not found");
            }
            const DependencyProperty* targetProperty =
                blueprint->properties->Find(
                    objects[targetIndex]->RuntimeType(),
                    setter->GetPropertyName());
            if (targetProperty == nullptr) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger Setter property was not found");
            }
            Base::Result<Value> converted =
                ConvertSetterValue(
                    *setter,
                    blueprint->nodes[targetIndex],
                    *targetProperty,
                    *blueprint->runtime,
                    *blueprint->properties);
            if (!converted) {
                return converted.GetStatus();
            }
            Aero::Controls::DataTemplateTriggerSetter
                runtimeSetter;
            runtimeSetter.target =
                Base::WeakRef<DependencyObject>(
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(
                            objects[targetIndex].Get())));
            runtimeSetter.property =
                targetProperty->Handle();
            runtimeSetter.value =
                std::move(converted).Value();
            Base::Result<void> added =
                runtimeTrigger.setters.PushBack(
                    std::move(runtimeSetter));
            if (!added) return added.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<TriggerBase>& authored :
         blueprint->dataTemplateTriggers) {
        if (!authored) continue;
        const Meta::TypeId triggerType =
            authored->RuntimeType();
        if (triggerType !=
                Trigger::StaticTypeId() &&
            triggerType != DataTrigger::StaticTypeId() &&
            triggerType !=
                MultiDataTrigger::StaticTypeId()) {
            continue;
        }
        Base::Result<
            Aero::Controls::DataTemplateTriggerState*>
            ensured = ensureTriggerContext();
        if (!ensured) return ensured.GetStatus();
        Aero::Controls::DataTemplatePropertyTrigger
            runtimeTrigger;
        Base::Span<const Base::Ref<Setter>>
            authoredSetters;
        if (triggerType ==
            Trigger::StaticTypeId()) {
            const auto& propertyTrigger =
                static_cast<const Trigger&>(
                    *authored);
            const DependencyProperty* sourceProperty =
                blueprint->properties->Find(
                    root->RuntimeType(),
                    propertyTrigger.GetPropertyName());
            if (sourceProperty == nullptr) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger source property was not found");
            }
            Aero::Controls::DataTemplateTriggerCondition
                condition;
            condition.source = Base::WeakRef<Base::Object>(root);
            condition.dependencySource =
                Base::WeakRef<DependencyObject>(
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(root.Get())));
            condition.property =
                sourceProperty->Handle();
            condition.value =
                propertyTrigger.GetAuthoredValue();
            if (condition.value.Kind() ==
                    ValueKind::String &&
                condition.value.Type() !=
                    sourceProperty->ValueType()) {
                Base::Result<Value> converted =
                    ConvertTemplateTextValue(
                        *blueprint->runtime,
                        *sourceProperty,
                        condition.value.AsString());
                if (!converted) {
                    return converted.GetStatus();
                }
                condition.value =
                    std::move(converted).Value();
            }
            Base::Result<void> added =
                runtimeTrigger.conditions.PushBack(
                    std::move(condition));
            if (!added) return added.GetStatus();
            authoredSetters =
                propertyTrigger.GetAuthoredSetters();
        } else if (triggerType ==
                   DataTrigger::StaticTypeId()) {
            const auto& dataTrigger =
                static_cast<const DataTrigger&>(
                    *authored);
            if (!dataTrigger.GetBinding()) {
                return InvalidTemplateCompiler(
                    "DataTemplate DataTrigger requires Binding");
            }
            Aero::Controls::DataTemplateTriggerCondition
                condition;
            condition.source = Base::WeakRef<Base::Object>(payload);
            condition.binding =
                dataTrigger.GetBinding();
            condition.value =
                dataTrigger.GetAuthoredValue();
            Base::Result<void> added =
                runtimeTrigger.conditions.PushBack(
                    std::move(condition));
            if (!added) return added.GetStatus();
            authoredSetters =
                dataTrigger.GetAuthoredSetters();
        } else {
            const auto& multi =
                static_cast<const MultiDataTrigger&>(
                    *authored);
            for (const Base::Ref<Condition>& authoredCondition :
                 multi.GetConditions()) {
                if (!authoredCondition ||
                    !authoredCondition->GetBinding()) {
                    return InvalidTemplateCompiler(
                        "DataTemplate MultiDataTrigger requires complete Conditions");
                }
                Aero::Controls::DataTemplateTriggerCondition
                    condition;
                condition.source = Base::WeakRef<Base::Object>(payload);
                condition.binding =
                    authoredCondition->GetBinding();
                condition.value =
                    authoredCondition->GetAuthoredValue();
                Base::Result<void> added =
                    runtimeTrigger.conditions.PushBack(
                        std::move(condition));
                if (!added) return added.GetStatus();
            }
            authoredSetters =
                multi.GetAuthoredSetters();
        }
        Base::Result<void> retained =
            appendRuntimeSetters(
                authoredSetters, runtimeTrigger);
        if (!retained) return retained.GetStatus();
        retained = runtimeTrigger.enterActions.Append(
                authored->GetEnterActions());
        if (retained) {
            retained =
                runtimeTrigger.exitActions.Append(
                    authored->GetExitActions());
        }
        if (retained) {
            retained =
                triggerContext->triggers.PushBack(
                    std::move(runtimeTrigger));
        }
        if (!retained) return retained.GetStatus();
    }
    if (triggerContext) {
        Base::Result<void> attached =
            Aero::ElementPrivate::AddAuthoredTrigger(
                static_cast<FrameworkElement&>(*root),
                Base::Ref<Base::Object>(triggerContext));
        if (!attached) return attached.GetStatus();
    }
    return root;
}

} // namespace Aero::Markup

#include "gui/MetadataInternal.hpp"
#include "markup/MarkupWriterInternal.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

// ===== StyleSupport =====


#include "gui/StyleInternal.hpp"

#include <Aero/Base/String.hpp>

#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Standard.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdio>
#include <new>
#include <utility>

namespace Aero::Markup {
using namespace Detail;

namespace {

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status MissingStyleProperty(
    const char* role,
    Base::StringView property,
    Core::TypeId targetType,
    const Core::TypeRegistry& types) noexcept {
    thread_local char message[384];
    const Core::TypeInfo* target =
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

const Core::DependencyProperty* ResolveStyleProperty(
    const Core::DependencyPropertyRegistry& properties,
    Core::TypeId targetType,
    Base::StringView name) noexcept {
    const Core::DependencyProperty* property =
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
    const Core::TypeInfo* owner =
        properties.Types().FindType(
            Core::AeroNamespaceUri(),
            name.Substr(0U, separator));
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

Base::Result<Core::PropertyValue> ToPropertyValue(
    const Core::Value& value,
    Core::TypeId expectedType) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML style value cannot be represented as a dependency-property value");
    }
    if (value.IsNullObject() && value.Type() != expectedType) {
        return Core::PropertyValue::NullObject(expectedType);
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
    const Core::TypeId target =
        static_cast<const Aero::Style&>(object).GetTargetType();
    if (target == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Implicit Style requires TargetType");
    }
    return Aero::ResourceKey::FromType(target);
}

} // namespace

namespace Detail {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const UiObjectModelOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlStyleSchemaFacet::Register(
    Schema& schema,
    Core::TypeId styleType,
    Core::TypeId setterType,
    Core::DependencyPropertyHandle styleProperty,
    Core::TypeId triggerType) noexcept {
    if (schema.IsFrozen() ||
        options_.properties == nullptr || !styleProperty.IsValid() ||
        !options_.properties->IsFrozen() || !options_.properties->Types().IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style extension registries are not ready");
    }
    if (schema_ != nullptr ||
        styleType == Core::InvalidTypeId || setterType == Core::InvalidTypeId) {
        return InvalidStyleXaml("XAML Style extension registration is invalid");
    }

    const Core::TypeInfo* styleInfo = schema.Types().FindType(styleType);
    const Core::TypeInfo* setterInfo = schema.Types().FindType(setterType);
    const Core::TypeInfo* triggerInfo =
        triggerType != Core::InvalidTypeId ? schema.Types().FindType(triggerType) : nullptr;
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Core::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Core::TypeFlags::ValueType) ||
        (triggerType != Core::InvalidTypeId &&
         (triggerInfo == nullptr || HasTypeFlag(triggerInfo->Flags(), Core::TypeFlags::ValueType)))) {
        return InvalidStyleXaml("XAML Style, Setter, and Trigger must be registered object types");
    }

    const Core::PropertyInfo* targetType =
        schema.Types().FindProperty(styleType, Base::StringView("TargetType"), false);
    const Core::PropertyInfo* basedOn =
        schema.Types().FindProperty(styleType, Base::StringView("BasedOn"), false);
    const Core::PropertyInfo* setters =
        schema.Types().FindProperty(styleType, Base::StringView("Setters"), false);
    const Core::PropertyInfo* property =
        schema.Types().FindProperty(setterType, Base::StringView("Property"), false);
    const Core::PropertyInfo* value =
        schema.Types().FindProperty(setterType, Base::StringView("Value"), false);
    if (targetType == nullptr || basedOn == nullptr || setters == nullptr ||
        property == nullptr || value == nullptr ||
        targetType->ValueType() !=
            Core::TypeOf<Core::TypeReference>() ||
        basedOn->ValueType() != styleType || setters->ValueType() != setterType ||
        property->ValueType() !=
            Core::TypeOf<Base::String>() ||
        value->ValueType() !=
            Core::TypeOf<Core::Value>() ||
        schema.Types().FindContentMember(styleType) != setters->Id()) {
        return InvalidStyleXaml("XAML Style metadata members are invalid");
    }

    const Core::PropertyInfo* triggers = nullptr;
    const Core::PropertyInfo* triggerProperty = nullptr;
    const Core::PropertyInfo* triggerValue = nullptr;
    const Core::PropertyInfo* triggerSetters = nullptr;
    if (triggerType != Core::InvalidTypeId) {
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
            triggers->ValueType() != triggerType ||
            triggerProperty->ValueType() !=
                Core::TypeOf<Base::String>() ||
            triggerValue->ValueType() !=
                Core::TypeOf<Core::Value>() ||
            triggerSetters->ValueType() != setterType) {
            return InvalidStyleXaml("XAML Trigger metadata members are invalid");
        }
    }

    const Core::DependencyProperty* styleDependency = options_.properties->Find(styleProperty);
    if (styleDependency == nullptr || styleDependency->ValueType() != styleType) {
        return InvalidStyleXaml("XAML Style property does not accept the registered Style type");
    }

    schema_ = &schema;
    styleType_ = styleType;
    setterType_ = setterType;
    triggerType_ = triggerType;
    Base::Result<void> styleAdapter =
        Detail::SchemaPrivate::AddType(schema, {
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

Base::Result<Core::PropertyValue> XamlStyleSchemaFacet::ConvertValueForProperty(
    const Core::Value& value,
    Core::TypeId targetType,
    Base::StringView propertyName) const noexcept {
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style conversion requires an initialized extension");
    }
    const Core::DependencyProperty* property =
        ResolveStyleProperty(
            *options_.properties,
            targetType,
            propertyName);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style property was not found on TargetType");
    }
    const Core::Value* candidate = &value;
    Base::Result<Core::Value> converted = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Style conversion was not attempted");
    if (candidate->Kind() == Core::ValueKind::String) {
        converted = schema_->ConvertText(property->ValueType(), candidate->AsString());
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    if (property->ValueType() == Aero::Length::StaticTypeId() &&
        candidate->Type() == Core::TypeOf<double>()) {
        Base::Result<double> numeric =
            Core::ValueCodec<double>::Decode(*candidate);
        if (!numeric) return numeric.GetStatus();
        converted = Core::ValueCodec<Aero::Length>::Encode(
            Aero::Length::Pixels(numeric.Value()));
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    if (property->ValueType() ==
            Media::Brush::StaticTypeId() &&
        candidate->Type() ==
            Core::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Core::ValueCodec<Base::Color>::Decode(
                *candidate);
        if (!color) return color.GetStatus();
        Base::Result<
            Base::Ref<Media::Brush>>
            brush =
                Media::MakeSolidColorBrush(
                    color.Value());
        if (!brush) return brush.GetStatus();
        return Core::Value::FromObject(
            Media::Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    if (property->ValueType() == Core::TypeOf<Base::Color>() &&
        candidate->Kind() == Core::ValueKind::Object &&
        options_.properties->Types().IsDerivedFrom(
            candidate->Type(), Media::Brush::StaticTypeId())) {
        Base::Result<Base::Ref<Media::Brush>> brush =
            Core::ValueCodec<Base::Ref<Media::Brush>>::Decode(
                *candidate);
        if (!brush) return brush.GetStatus();
        Media::Brush* source = brush.Value().Get();
        if (source == nullptr || source->RuntimeType() !=
                Media::SolidColorBrush::StaticTypeId()) {
            return InvalidStyleXaml(
                "Color-backed text property requires SolidColorBrush");
        }
        return Core::ValueCodec<Base::Color>::Encode(
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
    if (style.GetTargetType() == Core::InvalidTypeId) {
        Base::Result<void> defaultTarget = style.TrySetTargetType(
            Controls::Control::StaticTypeId());
        if (!defaultTarget) return defaultTarget.GetStatus();
    }
    const Core::TypeId targetType = style.GetTargetType();
    const Core::TypeInfo* targetInfo =
        options_.properties->Types().FindType(targetType);
    if (targetInfo == nullptr ||
        HasTypeFlag(
            targetInfo->Flags(),
            Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style TargetType must identify an object type");
    }
    for (const Base::Ref<Aero::Setter>& entry :
         style.GetAuthoredSetters()) {
        Aero::Setter* setter = entry.Get();
        if (setter == nullptr || !setter->IsAuthored()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property and Value");
        }
        const Core::DependencyProperty* property =
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
        Base::Result<Core::PropertyValue> value = ConvertValueForProperty(
            setter->GetAuthoredValue(),
            targetType,
            setter->GetPropertyName());
        if (!value) return value.GetStatus();
        Base::Result<void> resolved = setter->Resolve(
            property->Handle(), value.Value());
        if (!resolved) return resolved.GetStatus();
        Base::Result<void> added =
            style.TryAddSetter(*setter);
        if (!added) return added.GetStatus();
    }
    for (const Base::Ref<Aero::PropertyTrigger>& entry :
         style.GetAuthoredTriggers()) {
        Aero::PropertyTrigger* trigger =
            entry.Get();
        if (trigger == nullptr ||
            !trigger->IsAuthored()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Trigger requires Property, Value, and Setters");
        }
        const Core::DependencyProperty* condition =
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
        Base::Result<Core::PropertyValue> conditionValue = ConvertValueForProperty(
            trigger->GetAuthoredValue(),
            targetType,
            trigger->GetPropertyName());
        if (!conditionValue) return conditionValue.GetStatus();
        Aero::StylePropertyTrigger plan;
        plan.property = condition->Handle();
        plan.value = conditionValue.Value();
        Base::Result<void> actions =
            plan.enterActions.TryAppend(
                trigger->GetEnterActions());
        if (!actions) return actions.GetStatus();
        actions = plan.exitActions.TryAppend(
            trigger->GetExitActions());
        if (!actions) return actions.GetStatus();
        for (const Base::Ref<Aero::Setter>& setterEntry :
             trigger->GetAuthoredSetters()) {
            Aero::Setter* setter =
                setterEntry.Get();
            if (setter == nullptr ||
                !setter->IsAuthored()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style Trigger Setter requires Property and Value");
            }
            const Core::DependencyProperty* property =
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
            Base::Result<Core::PropertyValue> value = ConvertValueForProperty(
                setter->GetAuthoredValue(),
                targetType,
                setter->GetPropertyName());
            if (!value) return value.GetStatus();
            Base::Result<void> resolved =
                setter->Resolve(
                    property->Handle(),
                    value.Value());
            if (!resolved) {
                return resolved.GetStatus();
            }
            Base::Result<void> added = plan.setters.TryPushBack({
                property->Handle(), value.Value()});
            if (!added) return added.GetStatus();
        }
        Base::Result<void> added = style.TryAddPropertyTrigger(
            std::move(plan));
        if (!added) return added.GetStatus();
    }
    return Aero::Detail::StylePrivate::Seal(
        style, *options_.properties);
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

} // namespace Detail

struct UiObjectModel::Impl final {
    explicit Impl(
        const UiObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.metadata,
              *options.properties,
              options.allocator) {}

    Detail::XamlStyleSchemaFacet style;
    Detail::XamlTemplateSchemaFacet templates;
    bool registered = false;
};

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
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory != nullptr) {
        impl_ = new (memory) Impl(options);
    }
}

UiObjectModel::~UiObjectModel() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema) noexcept {
    UiObjectModelTypes types;
    types.style = Aero::Style::StaticTypeId();
    types.setter = Aero::Setter::StaticTypeId();
    types.trigger = Aero::PropertyTrigger::StaticTypeId();
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
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML UI object model allocation failed");
    }
    if (impl_->registered) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML UI object model is already registered");
    }
    Base::Result<void> registered =
        impl_->style.Register(
            schema,
            types.style,
            types.setter,
            types.styleProperty,
            types.trigger);
    if (registered && types.includeTemplates) {
        registered = impl_->templates.Register(schema);
    }
    if (registered) {
        impl_->registered = true;
    }
    return registered;
}

} // namespace Aero::Markup


// ===== TemplateSupport =====



#include <Aero/Base/String.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Styling.hpp>

#include "markup/MarkupInternal.hpp"


#include "../controls/TemplateInternals.hpp"

#include <new>
#include <utility>


namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Core;


class CompiledTemplateProgramOwner final
    : public Base::Object {
public:
    explicit CompiledTemplateProgramOwner(
        Detail::CompiledTemplateBlueprint blueprint) noexcept
        : blueprint_(std::move(blueprint)) {}
    ~CompiledTemplateProgramOwner() noexcept override = default;

    Detail::CompiledTemplateBlueprint& Blueprint() noexcept {
        return blueprint_;
    }

private:
    Detail::CompiledTemplateBlueprint blueprint_;
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
    Core::TypeId key = Core::InvalidTypeId;
    if (object.RuntimeType() == ControlTemplate::StaticTypeId()) {
        key = static_cast<const ControlTemplate&>(object).GetTargetType();
    } else if (object.RuntimeType() == DataTemplate::StaticTypeId()) {
        key = static_cast<const DataTemplate&>(object).GetDataType();
    }
    if (key == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

namespace Detail {

struct XamlTemplateSchemaFacet::Impl final {
    Impl(
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
        auto* self = static_cast<Impl*>(context);
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
            const Core::TypeId targetType =
                type == ControlTemplate::StaticTypeId()
                ? static_cast<ControlTemplate&>(object)
                      .GetTargetType()
                : static_cast<DataTemplate&>(object)
                      .GetDataType();
            // A keyed WPF ControlTemplate may deliberately omit TargetType.
            // Its target is inferred from the Style/Setter that consumes it,
            // so only validate an explicitly authored type here. The apply
            // path still checks that the eventual target is a Control.
            if (targetType != Core::InvalidTypeId) {
                const Core::TypeInfo* targetInfo =
                    self->runtime->Types().FindType(targetType);
                if (targetInfo == nullptr ||
                    TemplateHasTypeFlag(
                        targetInfo->Flags(),
                        Core::TypeFlags::ValueType)) {
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
                if (Controls::Detail::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = Controls::Detail::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       DataTemplate::StaticTypeId()) {
                auto& templateValue = static_cast<DataTemplate&>(object);
                if (Controls::Detail::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = Controls::Detail::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       ItemsPanelTemplate::StaticTypeId()) {
                auto& templateValue = static_cast<ItemsPanelTemplate&>(object);
                if (Controls::Detail::TemplatePrivate::BaseUri(templateValue).Empty()) {
                    baseUri = Controls::Detail::TemplatePrivate::SetBaseUri(templateValue, *services.baseUri);
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
                    &Controls::Detail::TemplatePrivate::AuthoredVisualTree(static_cast<DataTemplate&>(object));
            } else {
                authored =
                    &Controls::Detail::TemplatePrivate::AuthoredVisualTree(static_cast<ItemsPanelTemplate&>(object));
            }
            Base::Result<Detail::CompiledTemplateBlueprint>
                compiled =
                    Detail::CompileDeferredTemplateBlueprint(
                        *authored,
                        object.RuntimeType() ==
                                DataTemplate::StaticTypeId()
                            ? &Controls::Detail::TemplatePrivate::AuthoredNames(static_cast<DataTemplate&>(object))
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
                        dataTemplateTriggers.TryReserve(
                            Controls::Detail::TemplatePrivate::AuthoredTriggers(dataTemplate).Size());
                if (!reserved) {
                    return reserved.GetStatus();
                }
                for (const Base::Ref<
                         Aero::TriggerBase>& trigger :
                     Controls::Detail::TemplatePrivate::AuthoredTriggers(dataTemplate)) {
                    Base::Result<void> retained =
                        compiled.Value().
                            dataTemplateTriggers.
                                TryPushBack(trigger);
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
            Detail::CompiledTemplateBlueprint* programContext =
                &program.Value()->Blueprint();
            Base::Ref<Base::Object> programOwner =
                program.Value();
            Base::Result<void> configured;
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                configured = Controls::Detail::TemplatePrivate::Configure(dataTemplate,
                    &Detail::BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = Controls::Detail::TemplatePrivate::Seal(dataTemplate);
                }
            } else {
                auto& itemsPanel =
                    static_cast<ItemsPanelTemplate&>(object);
                configured = Controls::Detail::TemplatePrivate::Configure(itemsPanel,
                    &Detail::BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = Controls::Detail::TemplatePrivate::Seal(itemsPanel);
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
                Controls::Detail::TemplatePrivate::ClearAuthoredVisualTree(dataTemplate);
                Controls::Detail::TemplatePrivate::ClearAuthoredTriggers(dataTemplate);
                Controls::Detail::TemplatePrivate::ClearAuthoredNames(dataTemplate);
            } else {
                Controls::Detail::TemplatePrivate::ClearAuthoredVisualTree(
                    static_cast<ItemsPanelTemplate&>(object));
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        if (controlTemplate.GetTargetType() ==
            Core::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                Controls::Detail::TemplatePrivate::TrySetTargetType(controlTemplate,
                    Control::StaticTypeId());
            if (!inferred) return inferred.GetStatus();
        }
        Base::Result<Detail::CompiledTemplateDefinition>
            compiled =
                Detail::CompileControlTemplateDefinition(
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
        Detail::CompiledTemplateBlueprint* programContext =
            &program.Value()->Blueprint();
        Base::Ref<Base::Object> programOwner =
            program.Value();

        Base::Result<void> configured =
            Controls::Detail::TemplatePrivate::ConfigureFactory(controlTemplate,
                &Detail::BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    Controls::Detail::TemplatePrivate::TryAddTemplateBinding(controlTemplate,
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
                    Controls::Detail::TemplatePrivate::TryAddPropertyTrigger(controlTemplate,
                        std::move(trigger));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (VisualStateGroup& group :
                 compiled.Value().visualStateGroups) {
                configured =
                    Controls::Detail::TemplatePrivate::TryAddVisualStateGroup(controlTemplate,
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                Controls::Detail::TemplatePrivate::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        Controls::Detail::TemplatePrivate::ClearAuthoredVisualTree(controlTemplate);
        Controls::Detail::TemplatePrivate::ClearAuthoredVisualStateGroups(controlTemplate);
        Controls::Detail::TemplatePrivate::ClearAuthoredTriggers(controlTemplate);
        Controls::Detail::TemplatePrivate::ClearAuthoredNames(controlTemplate);
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
            ? Controls::Detail::TemplatePrivate::RegisterAuthoredName(
                  static_cast<ControlTemplate&>(scopeOwner), name, object)
            : Controls::Detail::TemplatePrivate::RegisterAuthoredName(
                  static_cast<DataTemplate&>(scopeOwner), name, object);
    }
};

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory != nullptr) {
        impl_ = new (memory) Impl(
            runtime,
            properties,
            *allocator_);
    }
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        impl_->schema != nullptr) {
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

    impl_->schema = &schema;
    Base::Result<void> status =
        Detail::SchemaPrivate::AddType(schema, {
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            &Impl::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = Detail::SchemaPrivate::AddType(schema, {
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            &Impl::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = Detail::SchemaPrivate::AddType(schema, {
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        impl_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Detail
} // namespace Aero::Markup


// ===== TemplateCompiler =====

#include "gui/StyleInternal.hpp"

#include "gui/BindingInternal.hpp"
#include "../controls/TemplateInternals.hpp"
#include "../runtime/DataTemplateTriggerState.hpp"
#include "../media/AnimationInternals.hpp"

#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Styling.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdio>
#include <utility>
#include "../controls/ControlBehavior.hpp"


namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;


Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

struct PendingPrototypeNode final {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    MemberId contentMember = InvalidMemberId;
};

bool ContainsObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (const PendingPrototypeNode& current :
         pending) {
        if (current.object.Get() == object) {
            return true;
        }
    }
    return false;
}

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
        pending.TryPushBack({
            visualTree,
            UINT32_MAX,
            InvalidMemberId});
    if (!appended) return appended.GetStatus();

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
            object->RuntimeType(), Visual::StaticTypeId());
        if ((index == 0U || source.parent != UINT32_MAX) &&
            !visual) {
            return InvalidTemplateCompiler(
                "Template VisualTree contains a non-Visual object");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentMember = source.contentMember;
        const Base::StringView name = names != nullptr
            ? names->NameOf(*object)
            : Base::StringView{};
        Base::Result<void> named =
            node.name.TryAssign(name);
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
                    DependencyObject::StaticTypeId()) &&
                RequiresPrototypeObject(
                    bindings, local.Value().AsObject().Get())) {
                prototypeProperty.objectNode =
                    FindPrototypeObject(
                        pending,
                        local.Value().AsObject().Get());
                if (prototypeProperty.objectNode == UINT32_MAX) {
                    prototypeProperty.objectNode = pending.Size();
                    appended = pending.TryPushBack({
                        local.Value().AsObject(),
                        UINT32_MAX,
                        InvalidMemberId});
                    if (!appended) return appended.GetStatus();
                }
            }
            appended = node.properties.TryPushBack(
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
            appended = node.gridColumns.TryAssign(
                grid.ColumnDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
            appended = node.gridRows.TryAssign(
                grid.RowDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (blueprint.contentPresenter == UINT32_MAX &&
            (runtime.Types().IsDerivedFrom(
                 node.type,
                 ContentPresenter::StaticTypeId()) ||
             runtime.Types().IsDerivedFrom(
                 node.type,
                 ScrollContentPresenter::
                     StaticTypeId()))) {
            blueprint.contentPresenter = index;
        }

        appended = blueprint.nodes.TryPushBack(
            std::move(node));
        if (!appended) return appended.GetStatus();

        for (const DeferredContentEdge& edge :
             edges) {
            if (edge.parent != object) continue;
            if (!edge.child ||
                ContainsObject(
                    pending, edge.child.Get())) {
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a cycle or duplicate");
            }
            appended = pending.TryPushBack({
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
        const std::uint32_t bindingSource =
            source.source != nullptr
            ? FindPrototypeObject(
                  pending, source.source)
            : UINT32_MAX;
        if (target == UINT32_MAX ||
            (source.source != nullptr &&
             bindingSource == UINT32_MAX) ||
            source.manager == nullptr ||
            source.metadata == nullptr) {
            return InvalidTemplateCompiler(
                "Deferred template Binding target or source is outside its VisualTree");
        }
        TemplatePrototypeBinding binding;
        binding.target = target;
        binding.source = bindingSource;
        binding.manager = source.manager;
        binding.metadata = source.metadata;
        binding.targetProperty =
            source.targetProperty;
        binding.dataContextProperty =
            source.dataContextProperty;
        binding.mode = source.mode;
        binding.updateSourceTrigger =
            source.updateSourceTrigger;
        Base::Result<void> assigned =
            binding.path.TryAssign(
                source.path.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = binding.stringFormat.TryAssign(
            source.stringFormat.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = blueprint.bindings.TryPushBack(
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
        // The runtime represents ToggleButton's nullable WPF IsChecked value
        // as a bool plus the read-only IsIndeterminate state. Preserve the
        // observable template condition `IsChecked == {x:Null}` by compiling
        // it to that state property instead of attempting to assign an object
        // null to a Boolean dependency property.
        if (authoredValue.IsNullObject() &&
            propertyName == Base::StringView("IsChecked") &&
            runtime.Types().IsDerivedFrom(
                targetType, Controls::Primitives::ToggleButton::StaticTypeId())) {
            const DependencyProperty* indeterminate = properties.Find(
                targetType, "IsIndeterminate");
            if (indeterminate == nullptr) {
                return InvalidTemplateCompiler(
                    "ToggleButton IsIndeterminate property was not found");
            }
            TemplateTriggerCondition condition;
            Base::Result<void> assigned =
                condition.sourceName.TryAssign(sourceName);
            if (!assigned) return assigned.GetStatus();
            condition.property = indeterminate->Handle();
            condition.value = Value::FromBoolean(
                TypeOf<bool>(), true);
            return trigger.conditions.TryPushBack(std::move(condition));
        }
        const DependencyProperty* property =
            propertyName == Base::StringView(
                "local:Element.IsFocusEngaged")
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
            condition.sourceName.TryAssign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        return trigger.conditions.TryPushBack(std::move(condition));
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
                    if (Controls::Detail::TemplatePrivate::AuthoredNames(controlTemplate).Find(
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
            Base::Result<void> assigned = setter.targetName.TryAssign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
            assigned = trigger.setters.TryPushBack(std::move(setter));
            if (!assigned) return assigned.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         Controls::Detail::TemplatePrivate::AuthoredTriggers(controlTemplate)) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == PropertyTrigger::StaticTypeId()) {
            const auto& source = static_cast<const PropertyTrigger&>(*object);
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
            compiled.TryPushBack(
                std::move(trigger));
        if (!added) return added.GetStatus();
    }
    return compiled;
}

Base::Result<Base::Vector<VisualStateGroup>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroup> groups;
    auto compileGroup = [&groups, &blueprint, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() !=
                XamlVisualStateGroupObject::
                    StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<
                const XamlVisualStateGroupObject&>(
                    *groupObject);
        if (sourceGroup.Name().Empty()) {
            return InvalidTemplateCompiler(
                "VisualStateGroup requires Name");
        }
        VisualStateGroup group;
        Base::Result<void> assigned =
            group.name.TryAssign(
                sourceGroup.Name());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<Base::Object>& stateObject :
             sourceGroup.States()) {
            if (!stateObject ||
                stateObject->RuntimeType() !=
                    XamlVisualStateObject::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<
                    const XamlVisualStateObject&>(
                        *stateObject);
            if (sourceState.Name().Empty()) {
                return InvalidTemplateCompiler(
                    "VisualState requires Name");
            }
            VisualState state;
            assigned = state.name.TryAssign(
                sourceState.Name());
            if (!assigned) {
                return assigned.GetStatus();
            }
            state.storyboard =
                sourceState.StoryboardValue();

            for (const Base::Ref<Base::Object>&
                     setterObject :
                 sourceState.Setters()) {
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

                VisualStateSetter setter;
                assigned =
                    setter.targetName.TryAssign(
                        sourceSetter.GetTargetName());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                setter.property = property->Handle();
                setter.value =
                    std::move(value).Value();
                assigned =
                    state.setters.TryPushBack(
                        std::move(setter));
                if (!assigned) {
                    return assigned.GetStatus();
                }
            }
            assigned =
                group.states.TryPushBack(
                    std::move(state));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        for (const Base::Ref<Base::Object>&
                 transitionObject :
             sourceGroup.Transitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() !=
                    XamlVisualTransitionObject::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition =
                static_cast<
                    const XamlVisualTransitionObject&>(
                        *transitionObject);
            if (sourceTransition.From().Empty() &&
                sourceTransition.To().Empty() &&
                sourceTransition.GeneratedDuration().Empty() &&
                !sourceTransition.StoryboardValue()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.From().Empty()) {
                bool found = false;
                for (const VisualState& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.From();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.To().Empty()) {
                bool found = false;
                for (const VisualState& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.To();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            VisualTransition transition;
            assigned = transition.from.TryAssign(
                sourceTransition.From());
            if (assigned) {
                assigned = transition.to.TryAssign(
                    sourceTransition.To());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GeneratedDuration().Empty()) {
                Media::Animation::Storyboard duration;
                assigned = duration.SetDuration(
                    sourceTransition.GeneratedDuration());
                if (!assigned) return assigned.GetStatus();
                transition.generatedDurationMicroseconds =
                    Aero::Detail::AnimationPrivate::Timing(duration).durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GeneratedEasingFunction();
            transition.storyboard =
                sourceTransition.StoryboardValue();
            assigned = group.transitions.TryPushBack(
                std::move(transition));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = groups.TryPushBack(
            std::move(group));
        if (!assigned) return assigned.GetStatus();
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         Controls::Detail::TemplatePrivate::AuthoredVisualStateGroups(controlTemplate)) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        Controls::Detail::TemplatePrivate::AuthoredVisualTree(controlTemplate);
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            ::Aero::DependencyObject::StaticTypeId())) {
        auto& root = static_cast<::Aero::DependencyObject&>(*authoredRoot);
        Base::Ref<Base::Object> valueStore = root.GetValueOr(
            XamlVisualStateManagerObject::
                VisualStateGroupStoreProperty,
            Base::Ref<Base::Object>{});
        if (valueStore && valueStore->RuntimeType() ==
                XamlVisualStates::StaticTypeId()) {
            for (const Base::Ref<Base::Object>& groupObject :
                 static_cast<XamlVisualStates&>(
                     *valueStore).Groups()) {
                Base::Result<void> compiled = compileGroup(groupObject);
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
        Controls::Detail::TemplatePrivate::AuthoredVisualTree(controlTemplate),
        &Controls::Detail::TemplatePrivate::AuthoredNames(controlTemplate),
        edges,
        bindings,
        runtime,
            properties);
    if (!blueprint) {
        return blueprint.GetStatus();
    }
    Base::Result<Base::Vector<VisualStateGroup>>
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
         Controls::Detail::TemplatePrivate::AuthoredTriggers(controlTemplate)) {
        if (!authored) continue;
        if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
            authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
            (authored->RuntimeType() == PropertyTrigger::StaticTypeId() &&
             (!static_cast<const PropertyTrigger&>(*authored).
                    GetEnterActions().Empty() ||
              !static_cast<const PropertyTrigger&>(*authored).
                    GetExitActions().Empty()))) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate instance trigger cannot be retained");
            }
            Base::Result<void> added =
                blueprint.Value().controlTemplateDataTriggers.TryPushBack(
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
                blueprint.Value().controlTemplateEventTriggers.TryPushBack(
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
            Base::Result<void> named = node.name.TryAssign(
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
            binding.targetName.TryAssign(
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
                TryPushBack(std::move(binding));
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
            BoxedItemValue::StaticTypeId()) {
        return DeferredTriggerValuesMatch(
            static_cast<const BoxedItemValue&>(
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
        if (!setter || !setter->IsAuthored()) {
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
        Base::Result<void> applied =
            dependencyObject->SetValue(
                property->Handle(),
                std::move(converted).Value());
        if (!applied) return applied.GetStatus();
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
        objects.TryReserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<Visual*> visuals;
    reserved = visuals.TryReserve(blueprint->nodes.Size());
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
        Visual* visual = blueprint->runtime->Types().IsDerivedFrom(
            node.type, Visual::StaticTypeId())
            ? static_cast<Visual*>(owner.Get()) : nullptr;
        Base::Result<void> added = objects.TryPushBack(std::move(owner));
        if (!added) return added.GetStatus();
        added = visuals.TryPushBack(visual);
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
            Base::Result<void> applied = dependencyObject.SetValue(
                property.property, std::move(value));
            if (!applied) return applied.GetStatus();
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            Base::Result<void> restored = grid.SetColumnDefinitions(
                node.gridColumns.AsSpan());
            if (restored) restored = grid.SetRowDefinitions(node.gridRows.AsSpan());
            if (!restored) return restored.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        if (node.parent != UINT32_MAX) {
            if (node.parent >= objects.Size() || visuals[index] == nullptr ||
                visuals[node.parent] == nullptr ||
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
        if (visuals[index] == nullptr) continue;
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
            Visual& contentHost =
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
        if (binding.manager == nullptr ||
            binding.metadata == nullptr ||
            binding.target >= visuals.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= visuals.Size())) {
            return InvalidTemplateCompiler(
                "Compiled template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
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
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<void> queued =
            binding.manager->QueueDeferred(
                descriptor);
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
            Aero::Detail::DataTemplateTriggerState>> created =
            Base::MakeRef<Aero::Detail::DataTemplateTriggerState>();
        if (!created) return created.GetStatus();
        Base::Ref<Aero::Detail::DataTemplateTriggerState> triggerContext =
            std::move(created).Value();
        triggerContext->root =
            static_cast<FrameworkElement*>(visuals[0U]);
        for (std::uint32_t index = 0U; index < visuals.Size(); ++index) {
            if (blueprint->nodes[index].name.Empty()) continue;
            Aero::Detail::DataTemplateTriggerState::NamedObject named;
            Base::Result<void> namedAssigned = named.name.TryAssign(
                blueprint->nodes[index].name.View());
            if (!namedAssigned) return namedAssigned.GetStatus();
            named.object = Base::Ref<Base::Object>::FromBorrowed(
                *static_cast<Base::Object*>(visuals[index]));
            namedAssigned = triggerContext->names.TryPushBack(
                std::move(named));
            if (!namedAssigned) return namedAssigned.GetStatus();
        }
        auto appendSetters =
            [&](Base::Span<const Base::Ref<Setter>> setters,
                Aero::Detail::DataTemplatePropertyTrigger& runtimeTrigger)
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
                Aero::Detail::DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(visuals[target]));
                runtimeSetter.property = property->Handle();
                runtimeSetter.value = std::move(value).Value();
                Base::Result<void> added = runtimeTrigger.setters.TryPushBack(
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
            // A control-template binding's Self, TemplatedParent, and
            // ancestor-at-the-template-boundary all resolve to the templated
            // control before the visual subtree is mounted.
            return static_cast<Base::Object*>(&context.TemplatedParent());
        };
        for (const Base::Ref<TriggerBase>& authored :
             blueprint->controlTemplateDataTriggers) {
            if (!authored) continue;
            Aero::Detail::DataTemplatePropertyTrigger runtimeTrigger;
            Base::Span<const Base::Ref<Setter>> setters;
            if (authored->RuntimeType() == PropertyTrigger::StaticTypeId()) {
                const auto& property =
                    static_cast<const PropertyTrigger&>(*authored);
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
                Aero::Detail::DataTemplateTriggerCondition condition;
                condition.source = Base::Ref<Base::Object>::FromBorrowed(
                    *source);
                condition.dependencySource =
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(source));
                condition.property = sourceProperty->Handle();
                Base::Result<Value> converted = ConvertTriggerValue(
                    property.GetAuthoredValue(), sourceType, *sourceProperty,
                    *blueprint->runtime, *blueprint->properties);
                if (!converted) return converted.GetStatus();
                condition.value = std::move(converted).Value();
                Base::Result<void> added = runtimeTrigger.conditions.TryPushBack(
                    std::move(condition));
                if (!added) return added.GetStatus();
                // Property-trigger setters continue to be owned by
                // TemplateEngine, which preserves their trigger precedence.
                // This per-instance plan supplies only the action lifecycle.
            } else if (authored->RuntimeType() == DataTrigger::StaticTypeId()) {
                const auto& data = static_cast<const DataTrigger&>(*authored);
                if (!data.GetBinding()) return InvalidTemplateCompiler(
                    "ControlTemplate DataTrigger requires Binding");
                Aero::Detail::DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*data.GetBinding());
                if (source != nullptr) {
                    condition.source =
                        Base::Ref<Base::Object>::FromBorrowed(*source);
                }
                condition.binding = data.GetBinding();
                condition.value = data.GetAuthoredValue();
                Base::Result<void> added = runtimeTrigger.conditions.TryPushBack(
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
                    Aero::Detail::DataTemplateTriggerCondition condition;
                    Base::Object* source = sourceFor(*authoredCondition->GetBinding());
                    if (source != nullptr) {
                        condition.source =
                            Base::Ref<Base::Object>::FromBorrowed(*source);
                    }
                    condition.binding = authoredCondition->GetBinding();
                    condition.value = authoredCondition->GetAuthoredValue();
                    Base::Result<void> added =
                        runtimeTrigger.conditions.TryPushBack(std::move(condition));
                    if (!added) return added.GetStatus();
                }
                setters = multi.GetAuthoredSetters();
            } else {
                continue;
            }
            Base::Result<void> configured = appendSetters(setters, runtimeTrigger);
            if (configured) {
                configured = runtimeTrigger.enterActions.TryAppend(
                    authored->GetEnterActions());
            }
            if (configured) {
                configured = runtimeTrigger.exitActions.TryAppend(
                    authored->GetExitActions());
            }
            if (configured) {
                configured = triggerContext->triggers.TryPushBack(
                    std::move(runtimeTrigger));
            }
            if (!configured) return configured.GetStatus();
        }
        Base::Result<void> attached =
            Aero::Detail::ElementPrivate::TryAddAuthoredTrigger(
                *triggerContext->root,
                Base::Ref<Base::Object>(std::move(triggerContext)));
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext) noexcept {
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
        objects.TryReserve(blueprint->nodes.Size());
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
        Base::Result<void> added = objects.TryPushBack(std::move(owner));
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
            Base::Result<void> applied = dependencyObject.SetValue(
                property.property, std::move(value));
            if (!applied) return applied.GetStatus();
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            Base::Result<void> restored = grid.SetColumnDefinitions(
                node.gridColumns.AsSpan());
            if (restored) restored = grid.SetRowDefinitions(
                node.gridRows.AsSpan());
            if (!restored) return restored.GetStatus();
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
        Base::Result<void> assigned =
            static_cast<FrameworkElement&>(*root)
                .SetDataContext(payload);
        if (!assigned) return assigned.GetStatus();
    }
    for (const TemplatePrototypeBinding& binding :
         blueprint->bindings) {
        if (binding.manager == nullptr ||
            binding.metadata == nullptr ||
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
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<void> queued =
            binding.manager->QueueDeferred(
                descriptor);
        if (!queued) return queued.GetStatus();
    }
    Base::Ref<Aero::Detail::DataTemplateTriggerState>
        triggerContext;
    auto ensureTriggerContext =
        [&]() noexcept
        -> Base::Result<
            Aero::Detail::DataTemplateTriggerState*> {
        if (triggerContext) {
            return triggerContext.Get();
        }
        Base::Result<Base::Ref<
            Aero::Detail::DataTemplateTriggerState>>
            created = Base::MakeRef<
                Aero::Detail::
                    DataTemplateTriggerState>();
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
            Aero::Detail::DataTemplateTriggerState::
                NamedObject named;
            Base::Result<void> assigned =
                named.name.TryAssign(
                    blueprint->nodes[index].name.View());
            if (!assigned) {
                return assigned.GetStatus();
            }
            named.object = objects[index];
            assigned =
                triggerContext->names.TryPushBack(
                    std::move(named));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        return triggerContext.Get();
    };
    auto appendRuntimeSetters =
        [&](Base::Span<const Base::Ref<Setter>> setters,
            Aero::Detail::DataTemplatePropertyTrigger&
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
            Aero::Detail::DataTemplateTriggerSetter
                runtimeSetter;
            runtimeSetter.target =
                Base::Ref<DependencyObject>::FromBorrowed(
                    *static_cast<DependencyObject*>(
                        objects[targetIndex].Get()));
            runtimeSetter.property =
                targetProperty->Handle();
            runtimeSetter.value =
                std::move(converted).Value();
            Base::Result<void> added =
                runtimeTrigger.setters.TryPushBack(
                    std::move(runtimeSetter));
            if (!added) return added.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<TriggerBase>& authored :
         blueprint->dataTemplateTriggers) {
        if (!authored) continue;
        const Core::TypeId triggerType =
            authored->RuntimeType();
        if (triggerType !=
                PropertyTrigger::StaticTypeId() &&
            triggerType != DataTrigger::StaticTypeId() &&
            triggerType !=
                MultiDataTrigger::StaticTypeId()) {
            continue;
        }
        Base::Result<
            Aero::Detail::DataTemplateTriggerState*>
            ensured = ensureTriggerContext();
        if (!ensured) return ensured.GetStatus();
        Aero::Detail::DataTemplatePropertyTrigger
            runtimeTrigger;
        Base::Span<const Base::Ref<Setter>>
            authoredSetters;
        if (triggerType ==
            PropertyTrigger::StaticTypeId()) {
            const auto& propertyTrigger =
                static_cast<const PropertyTrigger&>(
                    *authored);
            const DependencyProperty* sourceProperty =
                blueprint->properties->Find(
                    root->RuntimeType(),
                    propertyTrigger.GetPropertyName());
            if (sourceProperty == nullptr) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger source property was not found");
            }
            Aero::Detail::DataTemplateTriggerCondition
                condition;
            condition.source = root;
            condition.dependencySource =
                Base::Ref<DependencyObject>::FromBorrowed(
                    *static_cast<DependencyObject*>(
                        root.Get()));
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
                runtimeTrigger.conditions.TryPushBack(
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
            Aero::Detail::DataTemplateTriggerCondition
                condition;
            condition.source = payload;
            condition.binding =
                dataTrigger.GetBinding();
            condition.value =
                dataTrigger.GetAuthoredValue();
            Base::Result<void> added =
                runtimeTrigger.conditions.TryPushBack(
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
                Aero::Detail::DataTemplateTriggerCondition
                    condition;
                condition.source = payload;
                condition.binding =
                    authoredCondition->GetBinding();
                condition.value =
                    authoredCondition->GetAuthoredValue();
                Base::Result<void> added =
                    runtimeTrigger.conditions.TryPushBack(
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
        retained = runtimeTrigger.enterActions.TryAppend(
                authored->GetEnterActions());
        if (retained) {
            retained =
                runtimeTrigger.exitActions.TryAppend(
                    authored->GetExitActions());
        }
        if (retained) {
            retained =
                triggerContext->triggers.TryPushBack(
                    std::move(runtimeTrigger));
        }
        if (!retained) return retained.GetStatus();
    }
    if (triggerContext) {
        Base::Result<void> attached =
            Aero::Detail::ElementPrivate::TryAddAuthoredTrigger(
                static_cast<FrameworkElement&>(*root),
                Base::Ref<Base::Object>(triggerContext));
        if (!attached) return attached.GetStatus();
    }
    return root;
}

} // namespace Aero::Markup::Detail

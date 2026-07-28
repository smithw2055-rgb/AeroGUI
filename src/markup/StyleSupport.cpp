#include "PresentationObjectModelInternal.hpp"

#include <Aero/Base/String.hpp>
#include "SchemaInternal.hpp"
#include <Aero/Presentation/Rendering.hpp>

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

Presentation::ResourceDictionary* ResolveStyleResources(
    Base::Object& object,
    void*) noexcept {
    return object.RuntimeType() ==
            Presentation::Style::StaticTypeId()
        ? &static_cast<Presentation::Style&>(object).Resources()
        : nullptr;
}

Base::Result<Presentation::ResourceKey> ResolveStyleImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() != Presentation::Style::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implicit Style key requires a Style object");
    }
    const Core::TypeId target =
        static_cast<const Presentation::Style&>(object).TargetType();
    if (target == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Implicit Style requires TargetType");
    }
    return Presentation::ResourceKey::FromType(target);
}

} // namespace

namespace Detail {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const PresentationObjectModelOptions& options) noexcept
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
        Detail::SchemaAccess::AddType(schema, {
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
    const Core::DependencyProperty* property = options_.properties->Find(
        targetType, propertyName);
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
    return ToPropertyValue(*candidate, property->ValueType());
}

Base::Result<void> XamlStyleSchemaFacet::FinalizeStyle(
    Presentation::Style& style) noexcept {
    if (schema_ == nullptr || options_.properties == nullptr ||
        style.TargetType() == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style TargetType must be assigned before initialization completes");
    }
    const Core::TypeId targetType = style.TargetType();
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
    for (const Base::Ref<Presentation::Setter>& entry :
         style.AuthoredSetters()) {
        Presentation::Setter* setter = entry.Get();
        if (setter == nullptr || !setter->IsAuthored()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property and Value");
        }
        const Core::DependencyProperty* property = options_.properties->Find(
            targetType, setter->PropertyName());
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Style Setter property was not found on TargetType");
        }
        Base::Result<Core::PropertyValue> value = ConvertValueForProperty(
            setter->AuthoredValue(),
            targetType,
            setter->PropertyName());
        if (!value) return value.GetStatus();
        Base::Result<void> resolved = setter->Resolve(
            property->Handle(), value.Value());
        if (!resolved) return resolved.GetStatus();
        Base::Result<void> added =
            style.TryAddSetter(*setter);
        if (!added) return added.GetStatus();
    }
    for (const Base::Ref<Presentation::PropertyTrigger>& entry :
         style.AuthoredTriggers()) {
        Presentation::PropertyTrigger* trigger =
            entry.Get();
        if (trigger == nullptr ||
            !trigger->IsAuthored()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Trigger requires Property, Value, and Setters");
        }
        const Core::DependencyProperty* condition = options_.properties->Find(
            targetType, trigger->PropertyName());
        if (condition == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Style Trigger property was not found on TargetType");
        }
        Base::Result<Core::PropertyValue> conditionValue = ConvertValueForProperty(
            trigger->AuthoredValue(),
            targetType,
            trigger->PropertyName());
        if (!conditionValue) return conditionValue.GetStatus();
        Presentation::StylePropertyTrigger plan;
        plan.property = condition->Handle();
        plan.value = conditionValue.Value();
        for (const Base::Ref<Presentation::Setter>& setterEntry :
             trigger->AuthoredSetters()) {
            Presentation::Setter* setter =
                setterEntry.Get();
            if (setter == nullptr ||
                !setter->IsAuthored()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style Trigger Setter requires Property and Value");
            }
            const Core::DependencyProperty* property = options_.properties->Find(
                targetType, setter->PropertyName());
            if (property == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style Trigger Setter property was not found on TargetType");
            }
            Base::Result<Core::PropertyValue> value = ConvertValueForProperty(
                setter->AuthoredValue(),
                targetType,
                setter->PropertyName());
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
    return style.Seal(*options_.properties);
}

Base::Result<void> XamlStyleSchemaFacet::EndStyleInit(
    Base::Object& object,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style initialization requires an extension context");
    }
    return extension->FinalizeStyle(
        static_cast<Presentation::Style&>(
            object));
}

} // namespace Detail

struct PresentationObjectModel::Impl final {
    explicit Impl(
        const PresentationObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.runtime,
              *options.properties,
              options.allocator) {}

    Detail::XamlStyleSchemaFacet style;
    Detail::XamlTemplateSchemaFacet templates;
    bool registered = false;
};

PresentationObjectModel::PresentationObjectModel(
    const PresentationObjectModelOptions& options) noexcept
    : allocator_(options.allocator != nullptr
          ? options.allocator
          : &Base::GetDefaultAllocator()) {
    if (options.runtime == nullptr ||
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

PresentationObjectModel::~PresentationObjectModel() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> PresentationObjectModel::Register(
    Schema& schema) noexcept {
    PresentationObjectModelTypes types;
    types.style = Presentation::Style::StaticTypeId();
    types.setter = Presentation::Setter::StaticTypeId();
    types.trigger = Presentation::PropertyTrigger::StaticTypeId();
    types.styleProperty =
        Presentation::FrameworkElement::StyleProperty;
    return Register(schema, types);
}

Base::Result<void> PresentationObjectModel::Register(
    Schema& schema,
    const PresentationObjectModelTypes& types) noexcept {
    if (!optionsValid_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML Presentation object model options are invalid");
    }
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML Presentation object model allocation failed");
    }
    if (impl_->registered) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Presentation object model is already registered");
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

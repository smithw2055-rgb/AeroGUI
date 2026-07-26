#include "XamlPresentationObjectModelInternal.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Core::PropertyValue> ToPropertyValue(
    const XamlValue& value,
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

Base::Result<void> ResolveQualifiedType(
    const XamlServiceProvider& services,
    Base::StringView name,
    Core::TypeId& output) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return InvalidStyleXaml("Style TargetType is invalid");
    }

    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == ':') {
            if (colon != name.SizeBytes()) {
                return InvalidStyleXaml("Style TargetType contains multiple prefixes");
            }
            colon = index;
        }
    }

    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U || colon + 1U >= name.SizeBytes()) {
            return InvalidStyleXaml("Style TargetType prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(colon + 1U, name.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> ns = services.namespaces.Lookup(prefix);
    if (!ns) return ns.GetStatus();
    const Core::MetadataTypeDescriptor* type =
        services.schema->Descriptors().FindType(ns.Value(), localName);
    if (type == nullptr || HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style TargetType was not found or is not an object type");
    }
    output = type->Id();
    return {};
}

Presentation::ResourceDictionary* ResolveStyleResources(
    Base::Object& object,
    void*) noexcept {
    return object.RuntimeType() ==
            Presentation::Style::StaticTypeId()
        ? &static_cast<Presentation::Style&>(object).Resources()
        : nullptr;
}

Base::Result<ResourceKey> ResolveStyleImplicitKey(
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
    return ResourceKey::FromType(target);
}

} // namespace

namespace Detail {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const XamlPresentationObjectModelOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlStyleSchemaFacet::Register(
    XamlSchemaContext& schema,
    Core::ActivationProviderRegistry& activation,
    Core::TypeId styleType,
    Core::TypeId setterType,
    Core::DependencyPropertyHandle styleProperty,
    Core::TypeId triggerType) noexcept {
    if (schema.IsFrozen() || activation.IsFrozen() ||
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

    const Core::MetadataTypeDescriptor* styleInfo = schema.Descriptors().FindType(styleType);
    const Core::MetadataTypeDescriptor* setterInfo = schema.Descriptors().FindType(setterType);
    const Core::MetadataTypeDescriptor* triggerInfo =
        triggerType != Core::InvalidTypeId ? schema.Descriptors().FindType(triggerType) : nullptr;
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Core::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Core::TypeFlags::ValueType) ||
        (triggerType != Core::InvalidTypeId &&
         (triggerInfo == nullptr || HasTypeFlag(triggerInfo->Flags(), Core::TypeFlags::ValueType)))) {
        return InvalidStyleXaml("XAML Style, Setter, and Trigger must be registered object types");
    }

    const Core::MetadataPropertyDescriptor* targetType =
        schema.Descriptors().FindProperty(styleType, Base::StringView("TargetType"), false);
    const Core::MetadataPropertyDescriptor* basedOn =
        schema.Descriptors().FindProperty(styleType, Base::StringView("BasedOn"), false);
    const Core::MetadataPropertyDescriptor* setters =
        schema.Descriptors().FindProperty(styleType, Base::StringView("Setters"), false);
    const Core::MetadataPropertyDescriptor* property =
        schema.Descriptors().FindProperty(setterType, Base::StringView("Property"), false);
    const Core::MetadataPropertyDescriptor* value =
        schema.Descriptors().FindProperty(setterType, Base::StringView("Value"), false);
    if (targetType == nullptr || basedOn == nullptr || setters == nullptr ||
        property == nullptr || value == nullptr ||
        basedOn->ValueType() != styleType || setters->ValueType() != setterType ||
        schema.Facets().FindContentMember(styleType) != setters->Id()) {
        return InvalidStyleXaml("XAML Style metadata members are invalid");
    }

    const Core::MetadataPropertyDescriptor* triggers = nullptr;
    const Core::MetadataPropertyDescriptor* triggerProperty = nullptr;
    const Core::MetadataPropertyDescriptor* triggerValue = nullptr;
    const Core::MetadataPropertyDescriptor* triggerSetters = nullptr;
    if (triggerType != Core::InvalidTypeId) {
        triggers = schema.Descriptors().FindProperty(
            styleType, Base::StringView("Triggers"), false);
        triggerProperty = schema.Descriptors().FindProperty(
            triggerType, Base::StringView("Property"), false);
        triggerValue = schema.Descriptors().FindProperty(
            triggerType, Base::StringView("Value"), false);
        triggerSetters = schema.Descriptors().FindProperty(
            triggerType, Base::StringView("Setters"), false);
        if (triggers == nullptr || triggerProperty == nullptr ||
            triggerValue == nullptr || triggerSetters == nullptr ||
            triggers->ValueType() != triggerType ||
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
    styleProperty_ = styleProperty;
    targetTypeMember_ = targetType->Id();
    basedOnMember_ = basedOn->Id();
    settersMember_ = setters->Id();
    setterPropertyMember_ = property->Id();
    setterValueMember_ = value->Id();
    if (triggerType_ != Core::InvalidTypeId) {
        triggersMember_ = triggers->Id();
        triggerPropertyMember_ = triggerProperty->Id();
        triggerValueMember_ = triggerValue->Id();
        triggerSettersMember_ = triggerSetters->Id();
    }

    const XamlMemberAdapterRegistration coreMembers[] = {
        {targetTypeMember_, XamlMemberWriteMode::SetOnce, nullptr, this, &SetTargetType, true},
        {basedOnMember_, XamlMemberWriteMode::SetOnce, &SetBasedOn, this, nullptr},
        {settersMember_, XamlMemberWriteMode::Collection, &AddSetter, this, nullptr},
        {setterPropertyMember_, XamlMemberWriteMode::SetOnce, &SetSetterProperty, this, nullptr},
        {setterValueMember_, XamlMemberWriteMode::SetOnce, &SetSetterValue, this, nullptr, true},
        {styleProperty.value, XamlMemberWriteMode::SetOnce, nullptr, this, &SetStyleMember}
    };
    for (const XamlMemberAdapterRegistration& member : coreMembers) {
        Base::Result<void> registered = schema.TryRegisterMemberAdapter(member);
        if (!registered) return registered.GetStatus();
    }
    if (triggerType_ != Core::InvalidTypeId) {
        const XamlMemberAdapterRegistration triggerMembers[] = {
            {triggersMember_, XamlMemberWriteMode::Collection, &AddTrigger, this, nullptr},
            {triggerPropertyMember_, XamlMemberWriteMode::SetOnce, &SetTriggerProperty, this, nullptr},
            {triggerValueMember_, XamlMemberWriteMode::SetOnce, &SetTriggerValue, this, nullptr, true},
            {triggerSettersMember_, XamlMemberWriteMode::Collection, &AddTriggerSetter, this, nullptr}
        };
        for (const XamlMemberAdapterRegistration& member : triggerMembers) {
            Base::Result<void> registered = schema.TryRegisterMemberAdapter(member);
            if (!registered) return registered.GetStatus();
        }
    }

    Base::Result<void> styleAdapter = schema.TryRegisterTypeAdapter({
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
    Base::Result<void> setterAdapter = schema.TryRegisterTypeAdapter({
        setterType_, nullptr, nullptr, nullptr, this});
    if (!setterAdapter) return setterAdapter.GetStatus();
    if (triggerType_ != Core::InvalidTypeId) {
        Base::Result<void> triggerAdapter = schema.TryRegisterTypeAdapter({
            triggerType_, nullptr, nullptr, nullptr, this});
        if (!triggerAdapter) return triggerAdapter.GetStatus();
    }
    Base::Result<void> styleActivation = activation.TryRegister({
        styleType_, &ActivateStyle, this});
    if (!styleActivation) return styleActivation.GetStatus();
    Base::Result<void> setterActivation = activation.TryRegister({
        setterType_, &ActivateSetter, this});
    if (!setterActivation) return setterActivation.GetStatus();
    if (triggerType_ != Core::InvalidTypeId) {
        return activation.TryRegister({triggerType_, &ActivateTrigger, this});
    }
    return {};
}

Base::Result<Core::PropertyValue> XamlStyleSchemaFacet::ConvertValueForProperty(
    const XamlValue& value,
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
    const XamlValue* candidate = &value;
    Base::Result<XamlValue> converted = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Style conversion was not attempted");
    if (candidate->Kind() == XamlValueKind::String) {
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

Base::Result<Base::Ref<Base::Object>> XamlStyleSchemaFacet::ActivateStyle(
    Core::TypeId requestedType,
    const XamlActivationContext&,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || requestedType != extension->styleType_) {
        return InvalidStyleXaml("XAML Style activation type is invalid");
    }
    Base::Result<Base::Ref<Presentation::Style>> created =
        Base::MakeRef<Presentation::Style>(
            Core::InvalidTypeId,
            nullptr,
            requestedType);
    if (!created) return created.GetStatus();
    if (created.Value()->RuntimeType() != requestedType) {
        return InvalidStyleXaml(
            "Style metadata type does not match Presentation::Style");
    }
    return Base::Ref<Base::Object>(std::move(created).Value());
}

Base::Result<Base::Ref<Base::Object>> XamlStyleSchemaFacet::ActivateSetter(
    Core::TypeId requestedType,
    const XamlActivationContext&,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || requestedType != extension->setterType_) {
        return InvalidStyleXaml("XAML Setter activation type is invalid");
    }
    Base::Result<Base::Ref<Presentation::Setter>> created =
        Base::MakeRef<Presentation::Setter>(
            requestedType);
    if (!created) return created.GetStatus();
    if (created.Value()->RuntimeType() != requestedType) {
        return InvalidStyleXaml(
            "Setter metadata type does not match Presentation::Setter");
    }
    return Base::Ref<Base::Object>(std::move(created).Value());
}

Base::Result<Base::Ref<Base::Object>> XamlStyleSchemaFacet::ActivateTrigger(
    Core::TypeId requestedType,
    const XamlActivationContext&,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || requestedType != extension->triggerType_) {
        return InvalidStyleXaml("XAML Trigger activation type is invalid");
    }
    Base::Result<Base::Ref<Presentation::PropertyTrigger>> created =
        Base::MakeRef<Presentation::PropertyTrigger>(
            requestedType);
    if (!created) return created.GetStatus();
    if (created.Value()->RuntimeType() != requestedType) {
        return InvalidStyleXaml(
            "Trigger metadata type does not match Presentation::PropertyTrigger");
    }
    return Base::Ref<Base::Object>(std::move(created).Value());
}

Base::Result<void> XamlStyleSchemaFacet::SetTargetType(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style TargetType requires an extension context");
    }
    if (value.Kind() == XamlValueKind::String) {
        Core::TypeId type = Core::InvalidTypeId;
        Base::Result<void> resolved = ResolveQualifiedType(
            services, value.AsString(), type);
        if (!resolved) return resolved.GetStatus();
        return static_cast<Presentation::Style&>(
            object).TrySetTargetType(type);
    }
    if (value.Kind() == XamlValueKind::UnsignedInteger &&
        extension->options_.typeReferenceType != Core::InvalidTypeId &&
        value.Type() == extension->options_.typeReferenceType) {
        const Core::TypeId type = value.AsUnsignedInteger();
        const Core::MetadataTypeDescriptor* info =
            services.schema != nullptr ? services.schema->Descriptors().FindType(type) : nullptr;
        if (info == nullptr || HasTypeFlag(info->Flags(), Core::TypeFlags::ValueType)) {
            return InvalidStyleXaml("Style TargetType token is invalid");
        }
        return static_cast<Presentation::Style&>(
            object).TrySetTargetType(type);
    }
    return InvalidStyleXaml("Style TargetType expects a type name or x:Type token");
}

Base::Result<void> XamlStyleSchemaFacet::SetBasedOn(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::Object ||
        !value.AsObject() || value.Type() != extension->styleType_) {
        return InvalidStyleXaml("Style BasedOn expects a Style object");
    }
    return static_cast<Presentation::Style&>(
        object).TrySetBasedOn(
            value.AsObject());
}

Base::Result<void> XamlStyleSchemaFacet::AddSetter(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::Object ||
        !value.AsObject() || value.Type() != extension->setterType_) {
        return InvalidStyleXaml("Style Setters expects a Setter object");
    }
    auto& setter =
        static_cast<Presentation::Setter&>(
            *value.AsObject());
    Base::Ref<Presentation::Setter> retained =
        Base::Ref<Presentation::Setter>::TryFromBorrowed(
            setter);
    if (!retained) {
        return InvalidStyleXaml(
            "Style Setter cannot be retained");
    }
    return static_cast<Presentation::Style&>(
        object).TryAddAuthoredSetter(
            std::move(retained));
}

Base::Result<void> XamlStyleSchemaFacet::AddTrigger(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || extension->triggerType_ == Core::InvalidTypeId ||
        value.Kind() != XamlValueKind::Object || !value.AsObject() ||
        value.Type() != extension->triggerType_) {
        return InvalidStyleXaml("Style Triggers expects a Trigger object");
    }
    auto& trigger =
        static_cast<Presentation::PropertyTrigger&>(
            *value.AsObject());
    Base::Ref<Presentation::PropertyTrigger> retained =
        Base::Ref<Presentation::PropertyTrigger>::
            TryFromBorrowed(trigger);
    if (!retained) {
        return InvalidStyleXaml(
            "Style Trigger cannot be retained");
    }
    return static_cast<Presentation::Style&>(
        object).TryAddAuthoredTrigger(
            std::move(retained));
}

Base::Result<void> XamlStyleSchemaFacet::SetSetterProperty(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr || value.Kind() != XamlValueKind::String) {
        return InvalidStyleXaml("Setter Property expects a string");
    }
    return static_cast<Presentation::Setter&>(
        object).SetPropertyName(
            value.AsString());
}

Base::Result<void> XamlStyleSchemaFacet::SetSetterValue(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr) {
        return InvalidStyleXaml("Setter Value requires an extension context");
    }
    return static_cast<Presentation::Setter&>(
        object).SetAuthoredValue(value);
}

Base::Result<void> XamlStyleSchemaFacet::SetTriggerProperty(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr || value.Kind() != XamlValueKind::String) {
        return InvalidStyleXaml("Trigger Property expects a string");
    }
    return static_cast<Presentation::PropertyTrigger&>(
        object).SetPropertyName(
            value.AsString());
}

Base::Result<void> XamlStyleSchemaFacet::SetTriggerValue(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    if (context == nullptr) {
        return InvalidStyleXaml("Trigger Value requires an extension context");
    }
    return static_cast<Presentation::PropertyTrigger&>(
        object).SetAuthoredValue(value);
}

Base::Result<void> XamlStyleSchemaFacet::AddTriggerSetter(
    Base::Object& object,
    const XamlValue& value,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::Object ||
        !value.AsObject() || value.Type() != extension->setterType_) {
        return InvalidStyleXaml("Trigger Setters expects a Setter object");
    }
    auto& setter =
        static_cast<Presentation::Setter&>(
            *value.AsObject());
    Base::Ref<Presentation::Setter> retained =
        Base::Ref<Presentation::Setter>::TryFromBorrowed(
            setter);
    if (!retained) {
        return InvalidStyleXaml(
            "Trigger Setter cannot be retained");
    }
    return static_cast<Presentation::PropertyTrigger&>(
        object).TryAddAuthoredSetter(
            std::move(retained));
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

Base::Result<void> XamlStyleSchemaFacet::SetStyleMember(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr || services.schema == nullptr ||
        value.Kind() != XamlValueKind::Object) {
        return InvalidStyleXaml("Style property expects a Style object or x:Null");
    }
    Base::Result<Core::DependencyObject*> targetResult =
        services.schema->ResolvePropertyTarget(object);
    if (!targetResult) return targetResult.GetStatus();
    Core::DependencyObject* target = targetResult.Value();
    if (&target->PropertyRegistry() != extension->options_.properties) {
        return InvalidStyleXaml("Style property target is not a registered DependencyObject");
    }
    if (!value.IsNullObject() && value.Type() != extension->styleType_) {
        return InvalidStyleXaml("Style property received an incompatible object type");
    }
    if (value.IsNullObject() || !value.AsObject()) {
        return target->ClearValue(
            extension->styleProperty_);
    }
    return target->SetValue(
        extension->styleProperty_,
        Core::PropertyValue::FromObject(
            extension->styleType_,
            value.AsObject()));
}

} // namespace Detail

struct XamlPresentationObjectModel::Impl final {
    explicit Impl(
        const XamlPresentationObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.runtime,
              *options.properties,
              options.allocator) {}

    Detail::XamlStyleSchemaFacet style;
    Detail::XamlTemplateSchemaFacet templates;
    bool registered = false;
};

XamlPresentationObjectModel::XamlPresentationObjectModel(
    const XamlPresentationObjectModelOptions& options) noexcept
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

XamlPresentationObjectModel::~XamlPresentationObjectModel() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> XamlPresentationObjectModel::Register(
    XamlSchemaContext& schema,
    Core::ActivationProviderRegistry& activation) noexcept {
    XamlPresentationObjectModelTypes types;
    types.style = Presentation::Style::StaticTypeId();
    types.setter = Presentation::Setter::StaticTypeId();
    types.trigger = Presentation::PropertyTrigger::StaticTypeId();
    types.styleProperty =
        Presentation::FrameworkElement::StyleProperty;
    return Register(schema, activation, types);
}

Base::Result<void> XamlPresentationObjectModel::Register(
    XamlSchemaContext& schema,
    Core::ActivationProviderRegistry& activation,
    const XamlPresentationObjectModelTypes& types) noexcept {
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
            activation,
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

void XamlPresentationObjectModel::SetTypeReferenceType(
    Core::TypeId type) noexcept {
    if (impl_ != nullptr && !impl_->registered) {
        impl_->style.SetTypeReferenceType(type);
    }
}

} // namespace Aero::Markup

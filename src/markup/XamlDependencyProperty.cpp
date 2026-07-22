#include <Aero/Markup/XamlDependencyProperty.hpp>

namespace Aero::Markup {
namespace {

constexpr const char* MessageBridgeFrozen =
    "XAML dependency-property bridge must be configured before schema freeze";
constexpr const char* MessageInvalidTypeRegistration =
    "XAML dependency-object type registration is invalid";
constexpr const char* MessageRegistryNotReady =
    "DependencyPropertyRegistry and TypeRegistry must be frozen before bridging";
constexpr const char* MessageTargetNotDependencyObject =
    "XAML target object is not registered as a DependencyObject";
constexpr const char* MessagePropertyNotBridged =
    "XAML member is not mapped to a dependency property";
constexpr const char* MessageUnsupportedValue =
    "XAML value kind cannot be represented by PropertyValue";
constexpr const char* MessageRuntimeMismatch =
    "XAML dependency-object runtime metadata does not match the activation result";

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

XamlDependencyPropertyBridge::XamlDependencyPropertyBridge(
    XamlSchemaContext& schema,
    Core::DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : schema_(&schema),
      properties_(&properties),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_) {}

Base::Result<void> XamlDependencyPropertyBridge::TryRegisterType(
    const XamlDependencyObjectTypeRegistration& registration) noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageBridgeFrozen);
    }
    const Core::TypeInfo* type = schema_->Types().FindType(registration.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType) ||
        registration.cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidTypeRegistration);
    }
    for (const XamlDependencyObjectTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML dependency-object type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t>
XamlDependencyPropertyBridge::TryRegisterProperties() noexcept {
    if (schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageBridgeFrozen);
    }
    if (!schema_->Types().IsFrozen() || !properties_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageRegistryNotReady);
    }
    if (providerRegistered_) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML dependency-property provider is already registered");
    }

    std::uint32_t registered = 0U;
    for (const Core::TypeInfo& type : schema_->Types().Types()) {
        for (const Core::PropertyInfo& member : type.Properties()) {
            const Core::DependencyProperty* property = properties_->Find(
                type.Id(),
                member.Name());
            if (property == nullptr ||
                schema_->FindMemberAdapter(member.Id()) != nullptr) {
                continue;
            }
            if (registered == UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "XAML dependency-property adapter count overflow");
            }
            ++registered;
        }
    }

    XamlMemberProviderRegistration provider;
    provider.handles = &XamlDependencyPropertyBridge::HandlesDependencyProperty;
    provider.set = &XamlDependencyPropertyBridge::SetDependencyProperty;
    provider.context = this;
    Base::Result<void> providerResult =
        schema_->TryRegisterMemberProvider(provider);
    if (!providerResult) {
        return providerResult.GetStatus();
    }
    registeredPropertyCount_ = registered;
    providerRegistered_ = true;
    return registered;
}

bool XamlDependencyPropertyBridge::IsTypeRegistered(
    Core::TypeId type) const noexcept {
    return FindTypeRegistration(type) != nullptr;
}

const XamlDependencyObjectTypeRegistration*
XamlDependencyPropertyBridge::FindTypeRegistration(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        for (const XamlDependencyObjectTypeRegistration& registration : types_) {
            if (registration.type == current) {
                return &registration;
            }
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) {
            break;
        }
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::PropertyValue>
XamlDependencyPropertyBridge::ConvertValue(
    const XamlValue& value,
    const Core::DependencyProperty& property) const noexcept {
    switch (value.Kind()) {
    case XamlValueKind::Boolean:
        return Core::PropertyValue::FromBoolean(
            property.ValueType(),
            value.AsBoolean());
    case XamlValueKind::SignedInteger:
        return Core::PropertyValue::FromSignedInteger(
            property.ValueType(),
            value.AsSignedInteger());
    case XamlValueKind::UnsignedInteger:
        return Core::PropertyValue::FromUnsignedInteger(
            property.ValueType(),
            value.AsUnsignedInteger());
    case XamlValueKind::Double:
        return Core::PropertyValue::FromDouble(
            property.ValueType(),
            value.AsDouble());
    case XamlValueKind::Object:
        if (value.IsNullObject()) {
            return Core::PropertyValue::NullObject(property.ValueType());
        }
        return Core::PropertyValue::FromObject(
            value.Type(),
            value.AsObject());
    case XamlValueKind::String:
    case XamlValueKind::None:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageUnsupportedValue);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        MessageUnsupportedValue);
}

bool XamlDependencyPropertyBridge::HandlesDependencyProperty(
    const XamlResolvedMember& member,
    void* context) noexcept {
    auto* bridge = static_cast<XamlDependencyPropertyBridge*>(context);
    return bridge != nullptr && member.kind == Core::MemberKind::Property &&
        bridge->properties_->Find({member.id}) != nullptr;
}

Base::Result<void> XamlDependencyPropertyBridge::SetDependencyProperty(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    auto* bridge = static_cast<XamlDependencyPropertyBridge*>(context);
    if (bridge == nullptr || services.targetObject != &object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageTargetNotDependencyObject);
    }

    const Core::DependencyPropertyHandle propertyHandle{services.targetMember};
    const Core::DependencyProperty* property =
        bridge->properties_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessagePropertyNotBridged);
    }

    const XamlDependencyObjectTypeRegistration* type =
        bridge->FindTypeRegistration(services.targetObjectType);
    if (type == nullptr || type->cast == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageTargetNotDependencyObject);
    }
    Core::DependencyObject* dependencyObject = type->cast(
        object,
        type->context);
    if (dependencyObject == nullptr ||
        dependencyObject->RuntimeType() != services.targetObjectType ||
        &dependencyObject->PropertyRegistry() != bridge->properties_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageRuntimeMismatch);
    }

    Base::Result<Core::PropertyValue> converted =
        bridge->ConvertValue(value, *property);
    if (!converted) {
        return converted.GetStatus();
    }
    return dependencyObject->SetValue(
        propertyHandle,
        converted.Value());
}

} // namespace Aero::Markup

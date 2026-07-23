#include <Aero/Core/MemberAccessor.hpp>
#include <Aero/Core/DependencyProperty.hpp>

namespace Aero::Core {
namespace {

constexpr Base::Status UnsupportedPropertyStatus() noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "Property is external or structural and requires a specialized handler");
}

bool HasPropertyFlag(PropertyFlags value, PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Result<Value> GetDependencyProperty(
    const Base::Object& object,
    const PropertyInfo& property,
    Base::IAllocator&,
    void* context) noexcept {
    auto* registry = static_cast<DependencyPropertyRegistry*>(context);
    const auto& dependencyObject = static_cast<const DependencyObject&>(object);
    if (registry->Find(DependencyPropertyHandle{property.Id()}) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property metadata was not found");
    }
    return dependencyObject.GetValue(DependencyPropertyHandle{property.Id()});
}

Base::Result<void> SetDependencyProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value,
    void* context) noexcept {
    auto* registry = static_cast<DependencyPropertyRegistry*>(context);
    if (registry->Find(DependencyPropertyHandle{property.Id()}) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property metadata was not found");
    }
    return static_cast<DependencyObject&>(object).SetValue(
        DependencyPropertyHandle{property.Id()}, value);
}

} // namespace

MemberAccessor::MemberAccessor(
    TypeRegistry& types,
    Base::IAllocator* allocator) noexcept
    : types_(&types),
      allocator_(allocator != nullptr ? allocator : &types.Allocator()),
      providers_(allocator_) {}

Base::Result<void> MemberAccessor::TryRegisterProvider(
    const PropertyProviderRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "MemberAccessor is frozen");
    }
    if (registration.id == InvalidPropertyProviderId ||
        registration.objectType == InvalidTypeId ||
        (registration.get == nullptr && registration.set == nullptr)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Property provider registration is invalid");
    }
    for (const PropertyProviderRegistration& current : providers_) {
        if (current.id == registration.id) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "Property provider is already registered");
        }
    }
    return providers_.TryPushBack(registration);
}

Base::Result<void> MemberAccessor::Freeze() noexcept {
    if (frozen_) return {};
    if (!types_->IsFrozen()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before MemberAccessor");
    }
    for (const TypeInfo& type : types_->Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            if (property.Access() == PropertyAccessKind::Provider &&
                FindProvider(property.Provider()) == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::NotFound,
                    "Property provider required by metadata is not registered");
            }
        }
    }
    for (const PropertyProviderRegistration& provider : providers_) {
        if (types_->FindType(provider.objectType) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::NotFound,
                "Property provider object type is not registered");
        }
    }
    frozen_ = true;
    return {};
}

Base::Result<Value> MemberAccessor::GetProperty(
    const Base::Object& object,
    const PropertyInfo& property,
    Base::IAllocator* allocator) const noexcept {
    Base::Result<void> target = HasPropertyFlag(
        property.Flags(), PropertyFlags::Attached)
        ? (object.RuntimeType() != InvalidTypeId &&
           types_->FindType(object.RuntimeType()) != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Attached property target has no registered runtime type")))
        : ValidateTarget(object, property.OwnerType());
    if (!target) return target.GetStatus();
    if (HasPropertyFlag(property.Flags(), PropertyFlags::WriteOnly)) {
        return Base::Status::Failure(Base::ErrorCode::Unsupported,
            "Write-only property cannot be read");
    }
    Base::IAllocator& selected = allocator != nullptr ? *allocator : *allocator_;
    Base::Result<Value> result = UnsupportedPropertyStatus();
    if (property.Access() == PropertyAccessKind::Ordinary) {
        if (property.Getter() == nullptr) return UnsupportedPropertyStatus();
        result = property.Getter()(object, selected, property.Context());
    } else if (property.Access() == PropertyAccessKind::Provider) {
        const PropertyProviderRegistration* provider =
            FindProvider(property.Provider());
        if (provider == nullptr || provider->get == nullptr ||
            !types_->IsDerivedFrom(object.RuntimeType(), provider->objectType)) {
            return Base::Status::Failure(Base::ErrorCode::NotFound,
                "Readable property provider is not registered for the object");
        }
        result = provider->get(object, property, selected, provider->context);
    }
    if (result && (result.Value().IsUnset() ||
                   result.Value().Type() != property.ValueType())) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Property getter returned a value with the wrong type");
    }
    return result;
}

Base::Result<void> MemberAccessor::SetProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value) const noexcept {
    Base::Result<void> target = HasPropertyFlag(
        property.Flags(), PropertyFlags::Attached)
        ? (object.RuntimeType() != InvalidTypeId &&
           types_->FindType(object.RuntimeType()) != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Attached property target has no registered runtime type")))
        : ValidateTarget(object, property.OwnerType());
    if (!target) return target.GetStatus();
    const bool providerObjectAssignment =
        property.Access() == PropertyAccessKind::Provider &&
        value.Kind() == ValueKind::Object &&
        types_->IsDerivedFrom(value.Type(), property.ValueType());
    if (value.IsUnset() ||
        (value.Type() != property.ValueType() && !providerObjectAssignment)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Property value type does not match metadata");
    }
    if (HasPropertyFlag(property.Flags(), PropertyFlags::ReadOnly)) {
        return Base::Status::Failure(Base::ErrorCode::ReadOnly,
            "Read-only property cannot be written");
    }
    if (property.Access() == PropertyAccessKind::Ordinary) {
        return property.Setter() != nullptr
            ? property.Setter()(object, value, property.Context())
            : Base::Result<void>(UnsupportedPropertyStatus());
    }
    if (property.Access() == PropertyAccessKind::Provider) {
        const PropertyProviderRegistration* provider =
            FindProvider(property.Provider());
        if (provider == nullptr || provider->set == nullptr ||
            !types_->IsDerivedFrom(object.RuntimeType(), provider->objectType)) {
            return Base::Status::Failure(Base::ErrorCode::NotFound,
                "Writable property provider is not registered for the object");
        }
        return provider->set(object, property, value, provider->context);
    }
    return UnsupportedPropertyStatus();
}

Base::Result<Value> MemberAccessor::InvokeMethod(
    Base::Object& object,
    const MethodInfo& method,
    Base::Span<const Value> arguments,
    Base::IAllocator* allocator) const noexcept {
    Base::Result<void> target = ValidateTarget(object, method.OwnerType());
    if (!target) return target.GetStatus();
    if (arguments.Size() != method.Parameters().Size()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Method argument count does not match metadata");
    }
    for (std::uint32_t index = 0U; index < arguments.Size(); ++index) {
        if (arguments[index].IsUnset() ||
            arguments[index].Type() != method.Parameters()[index].Type()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Method argument type does not match metadata");
        }
    }
    Base::IAllocator& selected = allocator != nullptr ? *allocator : *allocator_;
    Base::Result<Value> result = method.Invoker()(
        object, arguments, selected, method.Context());
    if (!result) return result.GetStatus();
    if ((method.ReturnType() == InvalidTypeId && !result.Value().IsUnset()) ||
        (method.ReturnType() != InvalidTypeId &&
         (result.Value().IsUnset() ||
          result.Value().Type() != method.ReturnType()))) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Method returned a value with the wrong type");
    }
    return result;
}

const PropertyProviderRegistration* MemberAccessor::FindProvider(
    PropertyProviderId id) const noexcept {
    for (const PropertyProviderRegistration& provider : providers_) {
        if (provider.id == id) return &provider;
    }
    return nullptr;
}

Base::Result<void> MemberAccessor::ValidateTarget(
    const Base::Object& object,
    TypeId ownerType) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "MemberAccessor must be frozen before use");
    }
    if (object.RuntimeType() == InvalidTypeId ||
        !types_->IsDerivedFrom(object.RuntimeType(), ownerType)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Object runtime type is incompatible with the member owner");
    }
    return {};
}

Base::Result<void> TryRegisterDependencyPropertyProvider(
    MemberAccessor& accessor,
    DependencyPropertyRegistry& properties,
    TypeId dependencyObjectType) noexcept {
    PropertyProviderRegistration registration;
    registration.id = DependencyPropertyProviderId;
    registration.objectType = dependencyObjectType;
    registration.get = &GetDependencyProperty;
    registration.set = &SetDependencyProperty;
    registration.context = &properties;
    return accessor.TryRegisterProvider(registration);
}

} // namespace Aero::Core

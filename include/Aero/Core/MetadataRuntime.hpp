#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/MetadataDomain.hpp>

namespace Aero::Core {

using MetadataPropertyProviderGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    const MetadataPropertyDescriptor& property,
    void* context) noexcept;
using MetadataPropertyProviderSetCallback = Base::Result<void> (*)(
    Base::Object& object,
    const MetadataPropertyDescriptor& property,
    const Value& value,
    void* context) noexcept;

struct MetadataPropertyProviderRegistration final {
    PropertyProviderId id = InvalidPropertyProviderId;
    TypeId objectType = InvalidTypeId;
    MetadataPropertyProviderGetCallback get = nullptr;
    MetadataPropertyProviderSetCallback set = nullptr;
    void* context = nullptr;
};

// Runtime reflection facade backed only by immutable descriptors and typed
// facets. It intentionally does not inspect executable fields stored in the
// registration-time TypeRegistry.
class AERO_API MetadataRuntime final {
public:
    explicit MetadataRuntime(MetadataDomain& domain) noexcept
        : domain_(&domain), providers_() {}

    MetadataRuntime(const MetadataRuntime&) = delete;
    MetadataRuntime& operator=(const MetadataRuntime&) = delete;

    Base::Result<void> TryRegisterPropertyProvider(
        const MetadataPropertyProviderRegistration& registration) noexcept {
        if (frozen_) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "MetadataRuntime is frozen");
        }
        if (registration.id == InvalidPropertyProviderId ||
            registration.objectType == InvalidTypeId ||
            (registration.get == nullptr && registration.set == nullptr) ||
            domain_ == nullptr || !domain_->IsSealed() ||
            domain_->Descriptors().FindType(registration.objectType) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata property provider registration is invalid");
        }
        if (FindProvider(registration.id) != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata property provider is already registered");
        }
        return providers_.TryPushBack(registration);
    }

    Base::Result<void> Freeze() noexcept {
        if (frozen_) return {};
        if (domain_ == nullptr || !domain_->IsSealed() ||
            !domain_->Descriptors().IsSealed() ||
            !domain_->Facets().IsSealed() ||
            !domain_->Facets().ValueFacetsSealed()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "MetadataDomain and all typed facets must be sealed before MetadataRuntime");
        }
        frozen_ = true;
        return {};
    }

    bool IsFrozen() const noexcept { return frozen_; }
    MetadataDomain& Domain() const noexcept { return *domain_; }
    const MetadataDescriptorStore& Descriptors() const noexcept {
        return domain_->Descriptors();
    }
    const MetadataFacetStore& Facets() const noexcept {
        return domain_->Facets();
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        TypeId type) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        const TypeFactoryFacet* factory = Facets().FindTypeFactory(type);
        if (descriptor == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata type descriptor was not found");
        }
        if (HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ||
            HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
            factory == nullptr || factory->factory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Metadata type has no constructible factory facet");
        }
        Base::Result<Base::Ref<Base::Object>> created = factory->factory();
        if (!created) return created.GetStatus();
        if (!created.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Metadata factory facet returned a null object");
        }
        if (created.Value()->RuntimeType() != type) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata factory facet returned the wrong runtime type");
        }
        return created;
    }

    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        const ValueSemanticsFacet* facet = Facets().FindValueSemantics(type);
        if (descriptor == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata value type descriptor was not found");
        }
        if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
            facet == nullptr || facet->source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Metadata type has no value semantics facet");
        }
        return facet->source->TryCreateValue(type, source);
    }

    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        const TextConverterFacet* facet = Facets().FindTextConverter(type);
        if (descriptor == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata text target descriptor was not found");
        }
        if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
            facet == nullptr || facet->source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Metadata type has no text converter facet");
        }
        return facet->source->TryConvertText(type, text);
    }

    Base::Result<Value> GetProperty(
        const Base::Object& object,
        MemberId member) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataPropertyDescriptor* property =
            Descriptors().FindProperty(member);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata property descriptor was not found");
        }
        Base::Result<void> target = ValidatePropertyTarget(object, *property);
        if (!target) return target.GetStatus();
        if (HasPropertyFlag(property->Flags(), PropertyFlags::WriteOnly)) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Write-only metadata property cannot be read");
        }

        const PropertyAccessorFacet* accessor =
            Facets().FindPropertyAccessor(member);
        if (accessor == nullptr) return UnsupportedProperty();
        Base::Result<Value> value = UnsupportedProperty();
        if (accessor->access == PropertyAccessKind::Ordinary) {
            if (accessor->get == nullptr) return UnsupportedProperty();
            value = accessor->get(object, accessor->context);
        } else if (accessor->access == PropertyAccessKind::Provider) {
            const MetadataPropertyProviderRegistration* provider =
                FindProvider(accessor->provider);
            if (provider == nullptr || provider->get == nullptr ||
                !Descriptors().IsDerivedFrom(
                    object.RuntimeType(), provider->objectType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Readable metadata property provider was not found");
            }
            value = provider->get(object, *property, provider->context);
        }
        if (!value) return value.GetStatus();
        if (value.Value().IsUnset() ||
            value.Value().Type() != property->ValueType()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata getter returned an incompatible value");
        }
        return value;
    }

    Base::Result<void> SetProperty(
        Base::Object& object,
        MemberId member,
        const Value& value) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataPropertyDescriptor* property =
            Descriptors().FindProperty(member);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata property descriptor was not found");
        }
        Base::Result<void> target = ValidatePropertyTarget(object, *property);
        if (!target) return target.GetStatus();
        if (value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata property value is unset");
        }
        if (HasPropertyFlag(property->Flags(), PropertyFlags::ReadOnly)) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Read-only metadata property cannot be written");
        }

        const PropertyAccessorFacet* accessor =
            Facets().FindPropertyAccessor(member);
        if (accessor == nullptr) return UnsupportedProperty();
        const bool providerObjectAssignment =
            accessor->access == PropertyAccessKind::Provider &&
            value.Kind() == ValueKind::Object &&
            Descriptors().IsDerivedFrom(value.Type(), property->ValueType());
        if (value.Type() != property->ValueType() &&
            !providerObjectAssignment) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata property value type does not match the descriptor");
        }

        if (accessor->access == PropertyAccessKind::Ordinary) {
            return accessor->set != nullptr
                ? accessor->set(object, value, accessor->context)
                : Base::Result<void>(UnsupportedProperty());
        }
        if (accessor->access == PropertyAccessKind::Provider) {
            const MetadataPropertyProviderRegistration* provider =
                FindProvider(accessor->provider);
            if (provider == nullptr || provider->set == nullptr ||
                !Descriptors().IsDerivedFrom(
                    object.RuntimeType(), provider->objectType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Writable metadata property provider was not found");
            }
            return provider->set(object, *property, value, provider->context);
        }
        return UnsupportedProperty();
    }

    Base::Result<Value> InvokeMethod(
        Base::Object& object,
        MemberId member,
        Base::Span<const Value> arguments) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataMethodDescriptor* method =
            Descriptors().FindMethod(member);
        const MethodInvokerFacet* invoker =
            Facets().FindMethodInvoker(member);
        if (method == nullptr || invoker == nullptr ||
            invoker->invoke == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata method descriptor or invoker facet was not found");
        }
        if (!Descriptors().IsDerivedFrom(
                object.RuntimeType(), method->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object type is incompatible with the metadata method");
        }
        if (arguments.Size() != method->Parameters().Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata method argument count does not match");
        }
        for (std::uint32_t index = 0U; index < arguments.Size(); ++index) {
            if (arguments[index].IsUnset() ||
                arguments[index].Type() != method->Parameters()[index].Type()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Metadata method argument type does not match");
            }
        }
        Base::Result<Value> result = invoker->invoke(
            object, arguments, invoker->context);
        if (!result) return result.GetStatus();
        if ((method->ReturnType() == InvalidTypeId &&
             !result.Value().IsUnset()) ||
            (method->ReturnType() != InvalidTypeId &&
             (result.Value().IsUnset() ||
              result.Value().Type() != method->ReturnType()))) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata method returned an incompatible value");
        }
        return result;
    }

private:
    MetadataDomain* domain_ = nullptr;
    Base::Vector<MetadataPropertyProviderRegistration> providers_;
    bool frozen_ = false;

    static constexpr bool HasTypeFlag(
        TypeFlags value,
        TypeFlags flag) noexcept {
        return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
    }

    static constexpr bool HasPropertyFlag(
        PropertyFlags value,
        PropertyFlags flag) noexcept {
        return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
    }

    static Base::Status RuntimeNotFrozen() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataRuntime is not frozen");
    }

    static Base::Status UnsupportedProperty() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata property has no usable accessor facet");
    }

    Base::Result<void> ValidatePropertyTarget(
        const Base::Object& object,
        const MetadataPropertyDescriptor& property) const noexcept {
        if (HasPropertyFlag(property.Flags(), PropertyFlags::Attached)) {
            return object.RuntimeType() != InvalidTypeId &&
                Descriptors().FindType(object.RuntimeType()) != nullptr
                ? Base::Result<void>()
                : Base::Result<void>(Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Attached metadata property target has no descriptor"));
        }
        return Descriptors().IsDerivedFrom(
            object.RuntimeType(), property.OwnerType())
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object type is incompatible with the metadata property"));
    }

    const MetadataPropertyProviderRegistration* FindProvider(
        PropertyProviderId id) const noexcept {
        for (const MetadataPropertyProviderRegistration& provider : providers_) {
            if (provider.id == id) return &provider;
        }
        return nullptr;
    }
};

inline Base::Result<Value> GetDependencyPropertyFacetValue(
    const Base::Object& object,
    const MetadataPropertyDescriptor& property,
    void* context) noexcept {
    auto* registry = static_cast<DependencyPropertyRegistry*>(context);
    if (registry == nullptr ||
        registry->Find(DependencyPropertyHandle{property.Id()}) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property facet was not found");
    }
    return static_cast<const DependencyObject&>(object).GetValue(
        DependencyPropertyHandle{property.Id()});
}

inline Base::Result<void> SetDependencyPropertyFacetValue(
    Base::Object& object,
    const MetadataPropertyDescriptor& property,
    const Value& value,
    void* context) noexcept {
    auto* registry = static_cast<DependencyPropertyRegistry*>(context);
    if (registry == nullptr ||
        registry->Find(DependencyPropertyHandle{property.Id()}) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property facet was not found");
    }
    return static_cast<DependencyObject&>(object).SetValue(
        DependencyPropertyHandle{property.Id()}, value);
}

inline Base::Result<void> TryRegisterDependencyPropertyRuntimeProvider(
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties,
    TypeId dependencyObjectType) noexcept {
    return runtime.TryRegisterPropertyProvider({
        DependencyPropertyProviderId,
        dependencyObjectType,
        &GetDependencyPropertyFacetValue,
        &SetDependencyPropertyFacetValue,
        &properties});
}

} // namespace Aero::Core

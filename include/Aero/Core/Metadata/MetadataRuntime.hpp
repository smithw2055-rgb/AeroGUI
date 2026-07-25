#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

#include <cctype>
#include <cstdint>

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

class AERO_API MetadataRuntime final {
public:
    explicit MetadataRuntime(MetadataDomain& domain) noexcept
        : domain_(&domain), providers_() {}
    MetadataRuntime(const MetadataRuntime&) = delete;
    MetadataRuntime& operator=(const MetadataRuntime&) = delete;

    Base::Result<void> TryRegisterPropertyProvider(const MetadataPropertyProviderRegistration& registration) noexcept {
        if (frozen_) return Base::Status::Failure(Base::ErrorCode::InvalidState, "MetadataRuntime is frozen");
        if (registration.id == InvalidPropertyProviderId || registration.objectType == InvalidTypeId ||
            (registration.get == nullptr && registration.set == nullptr) || domain_ == nullptr || !domain_->IsSealed() ||
            domain_->Descriptors().FindType(registration.objectType) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata property provider registration is invalid");
        }
        if (FindProvider(registration.id) != nullptr) return Base::Status::Failure(Base::ErrorCode::AlreadyExists, "Metadata property provider is already registered");
        return providers_.TryPushBack(registration);
    }

    Base::Result<void> Freeze() noexcept {
        if (frozen_) return {};
        if (domain_ == nullptr || !domain_->IsSealed() || !domain_->Descriptors().IsSealed() ||
            !domain_->Facets().IsSealed() || !domain_->Facets().ValueFacetsSealed()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState, "MetadataDomain and all typed facets must be sealed before MetadataRuntime");
        }
        frozen_ = true;
        return {};
    }

    bool IsFrozen() const noexcept { return frozen_; }
    MetadataDomain& Domain() const noexcept { return *domain_; }
    const MetadataDescriptorStore& Descriptors() const noexcept { return domain_->Descriptors(); }
    const MetadataFacetStore& Facets() const noexcept { return domain_->Facets(); }

    Base::Result<Base::Ref<Base::Object>> CreateObject(TypeId type) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        const TypeFactoryFacet* factory = Facets().FindTypeFactory(type);
        if (descriptor == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata type descriptor was not found");
        if (descriptor->Kind() != MetadataTypeKind::Object || HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ||
            factory == nullptr || factory->factory == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::Unsupported, "Metadata type has no constructible factory facet");
        }
        Base::Result<Base::Ref<Base::Object>> created = factory->factory();
        if (!created) return created.GetStatus();
        if (!created.Value()) return Base::Status::Failure(Base::ErrorCode::InternalError, "Metadata factory facet returned a null object");
        if (created.Value()->RuntimeType() != type) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata factory facet returned the wrong runtime type");
        return created;
    }

    Base::Result<Value> TryCreateValue(TypeId type, const void* source) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        const ValueSemanticsFacet* facet = Facets().FindValueSemantics(type);
        if (descriptor == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata value type descriptor was not found");
        if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) || facet == nullptr || !facet->semantics) {
            return Base::Status::Failure(Base::ErrorCode::Unsupported, "Metadata type has no value semantics facet");
        }
        return Value::TryFromCustom(type, source, facet->semantics);
    }

    Base::Result<Value> TryConvertText(TypeId type, Base::StringView text) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataTypeDescriptor* descriptor = Descriptors().FindType(type);
        if (descriptor == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata text target descriptor was not found");
        if (descriptor->Kind() == MetadataTypeKind::Enum) return TryConvertEnumText(*descriptor, text);
        const TextConverterFacet* facet = Facets().FindTextConverter(type);
        if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) || facet == nullptr || facet->convert == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::Unsupported, "Metadata type has no text converter facet");
        }
        Base::Result<Value> converted = facet->convert(type, text, facet->context);
        if (!converted) return converted.GetStatus();
        if (converted.Value().IsUnset() || converted.Value().Type() != type) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata text converter facet returned an incompatible value");
        return converted;
    }

    Base::Result<Value> GetValueMember(const Value& owner, MemberId member) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataFieldDescriptor* field = Descriptors().FindField(member);
        const ValueMemberAccessorFacet* accessor = Facets().FindValueMemberAccessor(member);
        if (field == nullptr || accessor == nullptr || accessor->get == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata value field descriptor or accessor was not found");
        if (owner.Kind() != ValueKind::Custom || owner.Type() != field->OwnerType() || owner.AsCustom() == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is incompatible");
        Base::Result<Value> result = accessor->get(owner.AsCustom(), const_cast<MetadataRuntime&>(*this), accessor->context);
        if (!result) return result.GetStatus();
        if (result.Value().IsUnset() || result.Value().Type() != field->ValueType()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field getter returned an incompatible value");
        return result;
    }

    Base::Result<void> SetValueMember(Value& owner, MemberId member, const Value& value) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataFieldDescriptor* field = Descriptors().FindField(member);
        const ValueMemberAccessorFacet* accessor = Facets().FindValueMemberAccessor(member);
        if (field == nullptr || accessor == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata value field descriptor or accessor was not found");
        if (owner.Kind() != ValueKind::Custom || owner.Type() != field->OwnerType() || owner.AsCustom() == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field target is incompatible");
        if (HasFieldFlag(field->Flags(), FieldFlags::ReadOnly) || accessor->set == nullptr) return Base::Status::Failure(Base::ErrorCode::ReadOnly, "Read-only metadata value field cannot be written");
        if (value.IsUnset() || value.Type() != field->ValueType()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata value field value type does not match");
        Base::Result<Value> clone = TryCreateValue(owner.Type(), owner.AsCustom());
        if (!clone) return clone.GetStatus();
        owner = std::move(clone).Value();
        return accessor->set(owner.MutableCustom(), value, const_cast<MetadataRuntime&>(*this), accessor->context);
    }

    Base::Result<Value> GetProperty(const Base::Object& object, MemberId member) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataPropertyDescriptor* property = Descriptors().FindProperty(member);
        if (property == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata property descriptor was not found");
        Base::Result<void> target = ValidatePropertyTarget(object, *property);
        if (!target) return target.GetStatus();
        if (HasPropertyFlag(property->Flags(), PropertyFlags::WriteOnly)) return Base::Status::Failure(Base::ErrorCode::Unsupported, "Write-only metadata property cannot be read");
        const PropertyAccessorFacet* accessor = Facets().FindPropertyAccessor(member);
        if (accessor == nullptr) return UnsupportedProperty();
        Base::Result<Value> value = UnsupportedProperty();
        if (accessor->access == PropertyAccessKind::Ordinary) {
            if (accessor->get == nullptr) return UnsupportedProperty();
            value = accessor->get(object, accessor->context);
        } else if (accessor->access == PropertyAccessKind::Provider) {
            if (accessor->provider == DependencyPropertyProviderId) {
                value = GetDependencyProperty(object, *property);
            } else {
                const MetadataPropertyProviderRegistration* provider = FindProvider(accessor->provider);
                if (provider == nullptr || provider->get == nullptr || !Descriptors().IsAssignableFrom(provider->objectType, object.RuntimeType())) return Base::Status::Failure(Base::ErrorCode::NotFound, "Readable metadata property provider was not found");
                value = provider->get(object, *property, provider->context);
            }
        }
        if (!value) return value.GetStatus();
        if (value.Value().IsUnset() || value.Value().Type() != property->ValueType()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata getter returned an incompatible value");
        return value;
    }

    Base::Result<void> SetProperty(Base::Object& object, MemberId member, const Value& value) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataPropertyDescriptor* property = Descriptors().FindProperty(member);
        if (property == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata property descriptor was not found");
        Base::Result<void> target = ValidatePropertyTarget(object, *property);
        if (!target) return target.GetStatus();
        if (value.IsUnset()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata property value is unset");
        if (HasPropertyFlag(property->Flags(), PropertyFlags::ReadOnly)) return Base::Status::Failure(Base::ErrorCode::ReadOnly, "Read-only metadata property cannot be written");
        const PropertyAccessorFacet* accessor = Facets().FindPropertyAccessor(member);
        if (accessor == nullptr) return UnsupportedProperty();
        const bool objectAssignment = value.Kind() == ValueKind::Object && Descriptors().IsAssignableFrom(property->ValueType(), value.Type());
        if (value.Type() != property->ValueType() && !objectAssignment) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata property value type does not match the descriptor");
        if (accessor->access == PropertyAccessKind::Ordinary) return accessor->set != nullptr ? accessor->set(object, value, accessor->context) : Base::Result<void>(UnsupportedProperty());
        if (accessor->access == PropertyAccessKind::Provider) {
            if (accessor->provider == DependencyPropertyProviderId) {
                return SetDependencyProperty(object, *property, value);
            }
            const MetadataPropertyProviderRegistration* provider = FindProvider(accessor->provider);
            if (provider == nullptr || provider->set == nullptr || !Descriptors().IsAssignableFrom(provider->objectType, object.RuntimeType())) return Base::Status::Failure(Base::ErrorCode::NotFound, "Writable metadata property provider was not found");
            return provider->set(object, *property, value, provider->context);
        }
        return UnsupportedProperty();
    }

    Base::Result<Value> InvokeMethod(Base::Object& object, MemberId member, Base::Span<const Value> arguments) const noexcept {
        if (!frozen_) return RuntimeNotFrozen();
        const MetadataMethodDescriptor* method = Descriptors().FindMethod(member);
        const MethodInvokerFacet* invoker = Facets().FindMethodInvoker(member);
        if (method == nullptr || invoker == nullptr || invoker->invoke == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Metadata method descriptor or invoker facet was not found");
        if (!Descriptors().IsAssignableFrom(method->OwnerType(), object.RuntimeType())) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Object type is incompatible with the metadata method");
        if (arguments.Size() != method->Parameters().Size()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata method argument count does not match");
        for (std::uint32_t index = 0U; index < arguments.Size(); ++index) if (arguments[index].IsUnset() || arguments[index].Type() != method->Parameters()[index].Type()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata method argument type does not match");
        Base::Result<Value> result = invoker->invoke(object, arguments, invoker->context);
        if (!result) return result.GetStatus();
        if ((method->ReturnType() == InvalidTypeId && !result.Value().IsUnset()) ||
            (method->ReturnType() != InvalidTypeId && (result.Value().IsUnset() || result.Value().Type() != method->ReturnType()))) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Metadata method returned an incompatible value");
        return result;
    }

private:
    MetadataDomain* domain_ = nullptr;
    Base::Vector<MetadataPropertyProviderRegistration> providers_;
    bool frozen_ = false;
    static constexpr bool HasPropertyFlag(PropertyFlags value, PropertyFlags flag) noexcept { return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U; }
    static Base::Status RuntimeNotFrozen() noexcept { return Base::Status::Failure(Base::ErrorCode::InvalidState, "MetadataRuntime is not frozen"); }
    static Base::Status UnsupportedProperty() noexcept { return Base::Status::Failure(Base::ErrorCode::Unsupported, "Metadata property has no usable accessor facet"); }
    static Base::StringView Trim(Base::StringView value) noexcept {
        std::uint32_t begin = 0U; std::uint32_t end = value.SizeBytes();
        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1U]))) --end;
        return value.Substr(begin, end - begin);
    }
    static bool EqualsAsciiInsensitive(Base::StringView left, Base::StringView right) noexcept {
        if (left.SizeBytes() != right.SizeBytes()) return false;
        for (std::uint32_t index = 0U; index < left.SizeBytes(); ++index) if (std::tolower(static_cast<unsigned char>(left[index])) != std::tolower(static_cast<unsigned char>(right[index]))) return false;
        return true;
    }
    Base::Result<Value> TryConvertEnumText(const MetadataTypeDescriptor& type, Base::StringView input) const noexcept {
        Base::StringView remaining = Trim(input);
        if (remaining.Empty()) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Enum text is empty");
        std::uint64_t raw = 0U; std::uint32_t tokenCount = 0U;
        while (!remaining.Empty()) {
            std::uint32_t split = remaining.SizeBytes();
            for (std::uint32_t index = 0U; index < remaining.SizeBytes(); ++index) if (remaining[index] == ',' || remaining[index] == '|') { split = index; break; }
            const Base::StringView token = Trim(remaining.Substr(0U, split));
            if (token.Empty()) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Enum text contains an empty value");
            const MetadataEnumValueDescriptor* match = nullptr;
            for (MemberId member : type.EnumValues()) {
                const MetadataEnumValueDescriptor* candidate = Descriptors().FindEnumValue(member);
                if (candidate != nullptr && EqualsAsciiInsensitive(token, candidate->Name())) { match = candidate; break; }
            }
            if (match == nullptr) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Enum text value is not registered");
            ++tokenCount;
            if (!type.IsFlagsEnum() && tokenCount > 1U) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Non-flags enum accepts exactly one value");
            raw = type.IsFlagsEnum() ? (raw | match->RawValue()) : match->RawValue();
            if (split == remaining.SizeBytes()) break;
            remaining = Trim(remaining.Substr(split + 1U));
        }
        return HasTypeFlag(type.Flags(), TypeFlags::SignedEnum) ? Value::FromSignedInteger(type.Id(), static_cast<std::int64_t>(raw)) : Value::FromUnsignedInteger(type.Id(), raw);
    }
    Base::Result<void> ValidatePropertyTarget(const Base::Object& object, const MetadataPropertyDescriptor& property) const noexcept {
        if (HasPropertyFlag(property.Flags(), PropertyFlags::Attached)) return object.RuntimeType() != InvalidTypeId && Descriptors().FindType(object.RuntimeType()) != nullptr ? Base::Result<void>() : Base::Result<void>(Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Attached metadata property target has no descriptor"));
        return Descriptors().IsAssignableFrom(property.OwnerType(), object.RuntimeType()) ? Base::Result<void>() : Base::Result<void>(Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Object type is incompatible with the metadata property"));
    }
    Base::Result<Value> GetDependencyProperty(
        const Base::Object& object,
        const MetadataPropertyDescriptor& property) const noexcept {
        const DependencyPropertyHandle handle{property.Id()};
        DependencyPropertyRegistry& registry = domain_->DependencyProperties();
        if (!Descriptors().IsAssignableFrom(
                TypeOf<DependencyObject>(), object.RuntimeType()) ||
            registry.Find(handle) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Dependency property metadata target is invalid");
        }
        const auto& dependencyObject =
            static_cast<const DependencyObject&>(object);
        if (&dependencyObject.PropertyRegistry() != &registry) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Dependency property registry does not match metadata domain");
        }
        return dependencyObject.GetValue(handle);
    }
    Base::Result<void> SetDependencyProperty(
        Base::Object& object,
        const MetadataPropertyDescriptor& property,
        const Value& value) const noexcept {
        const DependencyPropertyHandle handle{property.Id()};
        DependencyPropertyRegistry& registry = domain_->DependencyProperties();
        if (!Descriptors().IsAssignableFrom(
                TypeOf<DependencyObject>(), object.RuntimeType()) ||
            registry.Find(handle) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Dependency property metadata target is invalid");
        }
        auto& dependencyObject = static_cast<DependencyObject&>(object);
        if (&dependencyObject.PropertyRegistry() != &registry) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Dependency property registry does not match metadata domain");
        }
        return dependencyObject.SetValue(handle, value);
    }
    const MetadataPropertyProviderRegistration* FindProvider(PropertyProviderId id) const noexcept { for (const MetadataPropertyProviderRegistration& provider : providers_) if (provider.id == id) return &provider; return nullptr; }
};

} // namespace Aero::Core

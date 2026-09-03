// Consolidated implementation. Keep sections ordered by dependency.

// ===== DependencyProperty =====

#include <Aero/DependencyProperty.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <cstdio>
#include <cstdint>
#include <limits>
#include <utility>

namespace Aero {

Base::Result<Meta::PropertyValue> NormalizeValueForProperty(
    Meta::Registry* metadata,
    const Meta::DependencyProperty& property,
    Meta::PropertyValue value) noexcept {
    const Meta::TypeId targetType = property.ValueType();
    if (property.AcceptsAnyValue() || value.Type() == targetType) {
        return value;
    }

    const Meta::TypeId nullableBooleanType =
        Meta::TypeOf<::Aero::Nullable<bool>>();
    if (targetType == nullableBooleanType) {
        if (value.Kind() == Meta::ValueKind::Boolean &&
            value.Type() == Meta::TypeOf<bool>()) {
            return Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(
                ::Aero::Nullable<bool>{value.AsBoolean()});
        }
        if (value.Kind() == Meta::ValueKind::Object &&
            value.IsNullObject()) {
            return Meta::ValueCodec<::Aero::Nullable<bool>>::Encode(
                ::Aero::Nullable<bool>{});
        }
    } else if (targetType == Meta::TypeOf<bool>() &&
        value.Type() == nullableBooleanType) {
        Base::Result<::Aero::Nullable<bool>> decoded =
            Meta::ValueCodec<::Aero::Nullable<bool>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        if (!decoded.Value().GetHasValue()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Indeterminate Nullable Boolean cannot be assigned to Boolean");
        }
        return Meta::PropertyValue::FromBoolean(
            targetType, decoded.Value().GetValue());
    }

    if (value.Kind() == Meta::ValueKind::String) {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Dependency-property text conversion requires metadata");
        }
        Base::Result<Meta::PropertyValue> converted =
            metadata->TryConvertText(targetType, value.AsString());
        if (!converted) return converted.GetStatus();
        value = std::move(converted).Value();
    }

    if (value.Kind() == Meta::ValueKind::Object &&
        value.Type() != targetType) {
        if (value.IsNullObject()) {
            value = Meta::PropertyValue::NullObject(targetType);
        } else if (value.AsObject()) {
            const Meta::TypeId objectType =
                value.AsObject()->RuntimeType() != Meta::InvalidTypeId
                    ? value.AsObject()->RuntimeType()
                    : value.Type();
            if (targetType == Meta::TypeOf<Base::Object>() ||
                (metadata != nullptr &&
                 metadata->Types().IsAssignableFrom(
                     targetType, objectType))) {
                value = Meta::PropertyValue::FromObject(
                    targetType,
                    Base::Ref<Base::Object>::FromBorrowed(*value.AsObject()));
            }
        }
    }

    if (value.Type() != targetType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Expression value cannot be assigned to the dependency property");
    }
    return value;
}

} // namespace Aero

namespace Aero::Meta {
namespace {

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

constexpr Base::Status ReadOnlyStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ReadOnly,
        "Dependency property is read-only");
}

constexpr Base::Status ValidationFailedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Dependency property value validation failed");
}

bool IsValidUpdateSourceTrigger(
    UpdateSourceTrigger trigger) noexcept {
    switch (trigger) {
    case UpdateSourceTrigger::Default:
    case UpdateSourceTrigger::PropertyChanged:
    case UpdateSourceTrigger::LostFocus:
    case UpdateSourceTrigger::Explicit:
        return true;
    }
    return false;
}

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

const DependencyProperty::MetadataEntry*
DependencyProperty::FindMetadataExact(TypeId forType) const noexcept {
    for (const MetadataEntry& entry : metadata_) {
        if (entry.forType == forType) {
            return &entry;
        }
    }
    return nullptr;
}

const PropertyMetadata* DependencyProperty::MetadataFor(
    TypeId forType) const noexcept {
    if (typeRegistry_ == nullptr || forType == InvalidTypeId) {
        return nullptr;
    }

    for (std::uint32_t i = 0U; i < 4U; ++i) {
        if (metadataCache_[i].forType == forType) {
            if (i > 0U) {
                const MetadataCacheEntry hit = metadataCache_[i];
                for (std::uint32_t j = i; j > 0U; --j) {
                    metadataCache_[j] = metadataCache_[j - 1U];
                }
                metadataCache_[0] = hit;
            }
            return metadataCache_[0].metadata;
        }
    }

    const PropertyMetadata* result = nullptr;
    TypeId current = forType;
    std::uint32_t remaining = typeRegistry_->TypeCount() + 1U;
    while (current != InvalidTypeId && remaining > 0U) {
        const MetadataEntry* exact = FindMetadataExact(current);
        if (exact != nullptr) {
            result = &exact->metadata;
            break;
        }

        const TypeInfo* type = typeRegistry_->FindType(current);
        if (type == nullptr) {
            break;
        }
        current = type->BaseType();
        --remaining;
    }
    if (result == nullptr && IsAttached()) {
        const MetadataEntry* owner = FindMetadataExact(registeredOwnerType_);
        if (owner != nullptr) {
            result = &owner->metadata;
        }
    }

    if (result != nullptr) {
        for (std::uint32_t j = 3U; j > 0U; --j) {
            metadataCache_[j] = metadataCache_[j - 1U];
        }
        metadataCache_[0].forType = forType;
        metadataCache_[0].metadata = result;
    }

    return result;
}

DependencyPropertyRegistry::DependencyPropertyRegistry(
    TypeRegistry& typeRegistry,
    BehaviorTable& behaviors) noexcept
    : typeRegistry_(&typeRegistry),
      behaviorRegistrations_(&behaviors),
      properties_(),
      memberIndex_() {}

Base::Result<void> DependencyPropertyRegistry::ValidateMetadata(
    TypeId valueType,
    DependencyPropertyFlags propertyFlags,
    const PropertyMetadata& metadata) const noexcept {
    if (metadata.defaultValue.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property metadata requires a default value");
    }
    if (!IsValidUpdateSourceTrigger(metadata.defaultUpdateSourceTrigger)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property metadata has an invalid update trigger");
    }

    DependencyProperty temporary;
    temporary.typeRegistry_ = typeRegistry_;
    temporary.valueType_ = valueType;
    temporary.flags_ = propertyFlags;
    return ValidateValue(temporary, metadata, metadata.defaultValue);
}

Base::Result<void> DependencyPropertyRegistry::ValidateValue(
    const DependencyProperty& property,
    const PropertyMetadata& metadata,
    const PropertyValue& value) const noexcept {
    if (value.IsUnset() || value.Type() == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property value is unset or has no type");
    }

    if (property.AcceptsAnyValue()) {
        if (typeRegistry_->FindType(value.Type()) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Dependency property value references an unregistered type");
        }
        if (!metadata.validate.Empty() &&
            !metadata.validate(value)) {
            return ValidationFailedStatus();
        }
        return {};
    }

    const TypeInfo* expected = typeRegistry_->FindType(property.ValueType());
    const TypeInfo* actual = typeRegistry_->FindType(value.Type());
    if (expected == nullptr || actual == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property value references an unregistered type");
    }

    const bool expectedValueType = IsValueType(*expected);
    const bool objectValue = value.Kind() == PropertyValueKind::Object;
    if (expectedValueType == objectValue) {
        thread_local char message[384]{};
        std::snprintf(
            message,
            sizeof(message),
            "Dependency property '%.*s' expects %s type '%.*s' but received %s value '%.*s'",
            static_cast<int>(property.Name().SizeBytes()),
            property.Name().Data(),
            expectedValueType ? "a value" : "an object",
            static_cast<int>(expected->Name().SizeBytes()),
            expected->Name().Data(),
            objectValue ? "an object" : "a value",
            static_cast<int>(actual->Name().SizeBytes()),
            actual->Name().Data());
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            message);
    }

    if (value.Type() != property.ValueType() &&
        !typeRegistry_->IsDerivedFrom(value.Type(), property.ValueType())) {
        thread_local char message[384]{};
        std::snprintf(
            message,
            sizeof(message),
            "Dependency property '%.*s' expects type '%.*s' but received '%.*s'",
            static_cast<int>(property.Name().SizeBytes()),
            property.Name().Data(),
            static_cast<int>(expected->Name().SizeBytes()),
            expected->Name().Data(),
            static_cast<int>(actual->Name().SizeBytes()),
            actual->Name().Data());
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            message);
    }

    if (expected->Kind() == MetadataTypeKind::Enum) {
        std::uint64_t raw = 0U;
        if (HasTypeFlag(expected->Flags(), TypeFlags::SignedEnum)) {
            if (value.Kind() != PropertyValueKind::SignedInteger) {
                return ValidationFailedStatus();
            }
            raw = static_cast<std::uint64_t>(
                value.AsSignedInteger());
        } else {
            if (value.Kind() != PropertyValueKind::UnsignedInteger) {
                return ValidationFailedStatus();
            }
            raw = value.AsUnsignedInteger();
        }
        if (!typeRegistry_->IsEnumValue(expected->Id(), raw)) {
            return ValidationFailedStatus();
        }
    }

    if (!metadata.validate.Empty() && !metadata.validate(value)) {
        return ValidationFailedStatus();
    }
    return {};
}

Base::Result<PropertyValue> DependencyPropertyRegistry::EvaluateValue(
    DependencyObject& object,
    const DependencyProperty& property,
    const PropertyMetadata& metadata,
    const PropertyValue& baseValue) const noexcept {
    Base::Result<void> validation = ValidateValue(
        property, metadata, baseValue);
    if (!validation) {
        return validation.GetStatus();
    }

    if (metadata.coerce.Empty()) {
        return baseValue;
    }

    Base::Result<PropertyValue> coerced = metadata.coerce(
        object, property, baseValue);
    if (!coerced) {
        return coerced.GetStatus();
    }

    validation = ValidateValue(property, metadata, coerced.Value());
    if (!validation) {
        return validation.GetStatus();
    }
    return std::move(coerced).Value();
}

PropertyFlags DependencyPropertyRegistry::ToTypeRegistryFlags(
    DependencyPropertyFlags propertyFlags,
    PropertyMetadataFlags metadataFlags) noexcept {
    PropertyFlags result = PropertyFlags::None;
    if (HasFlag(propertyFlags, DependencyPropertyFlags::Attached)) {
        result = result | PropertyFlags::Attached;
    }
    if (HasFlag(propertyFlags, DependencyPropertyFlags::ReadOnly)) {
        result = result | PropertyFlags::ReadOnly;
    }
    if (HasFlag(propertyFlags, DependencyPropertyFlags::AnyValue)) {
        result = result | PropertyFlags::AnyValue;
    }
    if (HasFlag(propertyFlags, DependencyPropertyFlags::Structural)) {
        result = result | PropertyFlags::Structural;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::Inherits)) {
        result = result | PropertyFlags::Inherits;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsMeasure)) {
        result = result | PropertyFlags::AffectsMeasure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsArrange)) {
        result = result | PropertyFlags::AffectsArrange;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsRender)) {
        result = result | PropertyFlags::AffectsRender;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentMeasure)) {
        result = result | PropertyFlags::AffectsParentMeasure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentArrange)) {
        result = result | PropertyFlags::AffectsParentArrange;
    }
    return result;
}

Base::Result<DependencyPropertyRegistrationResult>
DependencyPropertyRegistry::Register(
    const DependencyPropertyRegistration& registration) noexcept {
    if (frozen_ || typeRegistry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency properties must be registered before registry freeze");
    }
    if (registration.name.Empty() ||
        registration.ownerType == InvalidTypeId ||
        registration.valueType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property registration is incomplete");
    }
    if (typeRegistry_->FindType(registration.ownerType) == nullptr ||
        typeRegistry_->FindType(registration.valueType) == nullptr) {
        std::fprintf(
            stderr,
            "Dependency property registration missing type name=%.*s owner=%llu value=%llu owner-found=%u value-found=%u\n",
            static_cast<int>(registration.name.SizeBytes()),
            registration.name.Data(),
            static_cast<unsigned long long>(
                registration.ownerType),
            static_cast<unsigned long long>(
                registration.valueType),
            typeRegistry_->FindType(
                registration.ownerType) != nullptr
                ? 1U : 0U,
            typeRegistry_->FindType(
                registration.valueType) != nullptr
                ? 1U : 0U);
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property owner or value type is not registered");
    }
    if (typeRegistry_->FindProperty(
            registration.ownerType, registration.name, false) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "A property with the same owner and name is already registered");
    }

    Base::Result<void> validation = ValidateMetadata(
        registration.valueType,
        registration.flags,
        registration.metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    if (properties_.Size() == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Dependency property registry capacity limit reached");
    }

    DependencyProperty property;
    Base::Result<void> nameResult = property.name_.Assign(registration.name);
    if (!nameResult) {
        return nameResult.GetStatus();
    }
    property.typeRegistry_ = typeRegistry_;
    property.valueType_ = registration.valueType;
    property.registeredOwnerType_ = registration.ownerType;
    property.flags_ = registration.flags;

    DependencyProperty::MetadataEntry ownerMetadata;
    ownerMetadata.forType = registration.ownerType;
    ownerMetadata.owner = true;
    ownerMetadata.metadata = registration.metadata;
    Base::Result<void> metadataResult = property.metadata_.PushBack(
        std::move(ownerMetadata));
    if (!metadataResult) {
        return metadataResult.GetStatus();
    }

    Base::Result<void> reserveResult = properties_.Reserve(
        properties_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }
    reserveResult = memberIndex_.Reserve(memberIndex_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    if (property.GetIsReadOnly() &&
        nextReadOnlySecret_ == std::numeric_limits<std::uint64_t>::max()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Dependency property read-only key space is exhausted");
    }

    const MemberId member = MakeMemberId(
        registration.ownerType, MemberKind::Property, registration.name);
    property.handle_.value = member;
    if (property.GetIsReadOnly()) {
        property.readOnlySecret_ = Base::MixHash64(
            property.handle_.value ^ nextReadOnlySecret_);
        if (property.readOnlySecret_ == 0U) {
            property.readOnlySecret_ = 1U;
        }
    }

    const std::uint32_t propertyIndex = properties_.Size();
    Base::Result<void> appendResult = properties_.PushBack(
        std::move(property));
    AERO_ASSERT(appendResult);
    if (!appendResult) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved dependency property append unexpectedly failed");
    }

    Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> indexResult =
        memberIndex_.Insert(member, propertyIndex);
    AERO_ASSERT(indexResult && indexResult.Value().inserted);
    if (!indexResult || !indexResult.Value().inserted) {
        properties_.PopBack();
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved dependency property index insertion unexpectedly failed");
    }

    PropertyRegistration metaProperty;
    metaProperty.name = registration.name;
    metaProperty.valueType = registration.valueType;
    metaProperty.flags = ToTypeRegistryFlags(
        registration.flags, registration.metadata.flags);
    metaProperty.access = PropertyAccessKind::Provider;
    metaProperty.provider = DependencyPropertyProviderId;
    Base::Result<MemberId> registered = RegistrationTypes(
        *typeRegistry_, *behaviorRegistrations_).RegisterProperty(
            registration.ownerType, metaProperty);
    if (!registered) {
        static_cast<void>(memberIndex_.Erase(member));
        properties_.PopBack();
        return registered.GetStatus();
    }
    AERO_ASSERT(registered.Value() == member);
    if (properties_[propertyIndex].GetIsReadOnly()) {
        ++nextReadOnlySecret_;
    }

    DependencyPropertyRegistrationResult result;
    result.property.value = member;
    const DependencyProperty& stored = properties_[propertyIndex];
    if (stored.GetIsReadOnly()) {
        result.readOnlyKey.registry_ = this;
        result.readOnlyKey.property_ = result.property;
        result.readOnlyKey.secret_ = stored.readOnlySecret_;
    }
    return result;
}

Base::Result<void> DependencyPropertyRegistry::AddOwner(
    DependencyPropertyHandle propertyHandle,
    TypeId ownerType,
    const PropertyMetadata& metadata,
    DependencyPropertyFlags flags) noexcept {
    if (frozen_ || typeRegistry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency property owners must be added before registry freeze");
    }

    const std::uint32_t propertyIndex = FindIndex(propertyHandle.value);
    if (propertyIndex == InvalidIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    if (typeRegistry_->FindType(ownerType) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property owner type was not found");
    }

    DependencyProperty& property = properties_[propertyIndex];
    if (property.FindMetadataExact(ownerType) != nullptr ||
        typeRegistry_->FindProperty(ownerType, property.Name(), false) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Dependency property already has metadata for this owner");
    }

    property.flags_ = property.flags_ | flags;

    Base::Result<void> validation = ValidateMetadata(
        property.ValueType(),
        property.Flags(),
        metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    Base::Result<void> reserveResult = property.metadata_.Reserve(
        property.metadata_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }
    reserveResult = memberIndex_.Reserve(memberIndex_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    DependencyProperty::MetadataEntry entry;
    entry.forType = ownerType;
    entry.owner = true;
    entry.metadata = metadata;
    Base::Result<void> appendResult = property.metadata_.PushBack(
        std::move(entry));
    AERO_ASSERT(appendResult);
    if (!appendResult) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved owner metadata append unexpectedly failed");
    }

    Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> indexResult =
        memberIndex_.Insert(
            MakeMemberId(ownerType, MemberKind::Property, property.Name()),
            propertyIndex);
    AERO_ASSERT(indexResult && indexResult.Value().inserted);
    if (!indexResult || !indexResult.Value().inserted) {
        property.metadata_.PopBack();
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved owner alias insertion unexpectedly failed");
    }

    const MemberId aliasMember =
        MakeMemberId(ownerType, MemberKind::Property, property.Name());
    PropertyRegistration metaProperty;
    metaProperty.name = property.Name();
    metaProperty.valueType = property.ValueType();
    metaProperty.flags = ToTypeRegistryFlags(property.Flags() | flags, metadata.flags);
    metaProperty.access = PropertyAccessKind::Provider;
    metaProperty.provider = DependencyPropertyProviderId;
    Base::Result<MemberId> alias = RegistrationTypes(
        *typeRegistry_, *behaviorRegistrations_).RegisterProperty(
            ownerType, metaProperty);
    if (!alias) {
        static_cast<void>(memberIndex_.Erase(aliasMember));
        property.metadata_.PopBack();
        return alias.GetStatus();
    }
    AERO_ASSERT(alias.Value() == aliasMember);
    return {};
}

Base::Result<void> DependencyPropertyRegistry::OverrideMetadata(
    DependencyPropertyHandle propertyHandle,
    TypeId forType,
    const PropertyMetadata& metadata) noexcept {
    if (frozen_ || typeRegistry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency property metadata must be overridden before freeze");
    }

    const std::uint32_t propertyIndex = FindIndex(propertyHandle.value);
    if (propertyIndex == InvalidIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }

    const TypeInfo* type = typeRegistry_->FindType(forType);
    if (type == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata override type was not found");
    }

    DependencyProperty& property = properties_[propertyIndex];
    if (property.FindMetadataExact(forType) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Dependency property already has exact metadata for this type");
    }
    if (type->BaseType() == InvalidTypeId ||
        property.MetadataFor(type->BaseType()) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata can only be overridden on a derived property owner type");
    }

    Base::Result<void> validation = ValidateMetadata(
        property.ValueType(),
        property.Flags(),
        metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    Base::Result<void> reserveResult = property.metadata_.Reserve(
        property.metadata_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    DependencyProperty::MetadataEntry entry;
    entry.forType = forType;
    entry.owner = false;
    entry.metadata = metadata;
    return property.metadata_.PushBack(std::move(entry));
}

Base::Result<void> DependencyPropertyRegistry::Freeze() noexcept {
    if (frozen_) {
        return {};
    }
    if (!typeRegistry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before DependencyPropertyRegistry");
    }

    for (const DependencyProperty& property : properties_) {
        if (property.MetadataFor(property.RegisteredOwnerType()) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Dependency property is missing registered-owner metadata");
        }
        for (const DependencyProperty::MetadataEntry& entry : property.metadata_) {
            if (typeRegistry_->FindType(entry.forType) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Dependency property metadata references an unknown type");
            }
            Base::Result<void> validation = ValidateMetadata(
                property.ValueType(),
                property.Flags(),
                entry.metadata);
            if (!validation) {
                return validation.GetStatus();
            }
        }
    }

    frozen_ = true;
    return {};
}

std::uint32_t DependencyPropertyRegistry::FindIndex(MemberId member) const noexcept {
    const std::uint32_t* index = memberIndex_.Find(member);
    return index != nullptr ? *index : InvalidIndex;
}

const DependencyProperty* DependencyPropertyRegistry::Find(
    DependencyPropertyHandle property) const noexcept {
    const std::uint32_t index = FindIndex(property.value);
    return index != InvalidIndex ? &properties_[index] : nullptr;
}

const DependencyProperty* DependencyPropertyRegistry::Find(
    TypeId ownerType,
    Base::StringView name) const noexcept {
    const PropertyInfo* property = typeRegistry_->FindProperty(
        ownerType, name, true);
    if (property == nullptr) {
        return nullptr;
    }
    const std::uint32_t index = FindIndex(property->Id());
    return index != InvalidIndex ? &properties_[index] : nullptr;
}

Base::Result<void> DependencyPropertyRegistry::ValidateValueFor(
    DependencyPropertyHandle propertyHandle,
    TypeId ownerType,
    const PropertyValue& value) const noexcept {
    const DependencyProperty* property = Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    const PropertyMetadata* metadata = property->MetadataFor(ownerType);
    if (metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Dependency property does not apply to this type");
    }
    return ValidateValue(*property, *metadata, value);
}

bool DependencyPropertyRegistry::ValidateKey(
    DependencyPropertyHandle property,
    const DependencyPropertyKey* key) const noexcept {
    if (key == nullptr || key->registry_ != this ||
        key->property_ != property || key->secret_ == 0U) {
        return false;
    }

    const DependencyProperty* registered = Find(property);
    return registered != nullptr && registered->GetIsReadOnly() &&
        registered->readOnlySecret_ == key->secret_;
}

} // namespace Aero::Meta

namespace Aero {

using namespace Meta;

DependencyObject::MutationScope::MutationScope(
    DependencyObject* owner,
    DispatcherReentrancyGuard&& guard) noexcept
    : owner_(owner),
      dispatcherGuard_(std::move(guard)) {}

DependencyObject::MutationScope::MutationScope(
    MutationScope&& other) noexcept
    : owner_(other.owner_),
      dispatcherGuard_(std::move(other.dispatcherGuard_)) {
    other.owner_ = nullptr;
}

DependencyObject::MutationScope& DependencyObject::MutationScope::operator=(
    MutationScope&& other) noexcept {
    if (this != &other) {
        Release();
        owner_ = other.owner_;
        dispatcherGuard_ = std::move(other.dispatcherGuard_);
        other.owner_ = nullptr;
    }
    return *this;
}

DependencyObject::MutationScope::~MutationScope() {
    Release();
}

void DependencyObject::MutationScope::Release() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    owner_->LeaveMutation();
    owner_ = nullptr;
    dispatcherGuard_.Release();
}

DependencyObject::DependencyObject(TypeId runtimeType) noexcept
    : DispatcherObject(*CurrentObjectFactory().dispatcher),
      registry_(CurrentObjectFactory().dependencyProperties),
      runtimeType_(runtimeType),
      objectServicesAvailable_(HasObjectFactory()),
      valueStore_(nullptr),
      updateStack_(),
      changeHandlers_() {}

DependencyObject::~DependencyObject() {
    PropertyStore* store = static_cast<PropertyStore*>(valueStore_);
    if (store != nullptr) {
        for (auto& record : store->entries) {
            AeroGuiInternal::CommitConsumerChange(
                *this,
                DependencyPropertyHandle{record.Key()},
                record.Value().effectiveValue,
                PropertyValue::Unset());
            ReleaseExpression(record.Value());
        }
        delete store;
        valueStore_ = nullptr;
    }
}




















































} // namespace Aero


// ===== EffectiveValueEngine =====




namespace Aero::Meta {
namespace {

constexpr std::uint32_t EffectiveInvalidIndex = UINT32_MAX;

[[maybe_unused]] Base::Status InvalidProviderStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "The property provider rank is not a mutable base-value source");
}

class FlushScope {
public:
    explicit FlushScope(bool& flag) noexcept : flag_(&flag) {
        flag = true;
    }

    ~FlushScope() {
        *flag_ = false;
    }

    FlushScope(const FlushScope&) = delete;
    FlushScope& operator=(const FlushScope&) = delete;

private:
    bool* flag_ = nullptr;
};

} // namespace

EffectiveValueEngine::EffectiveValueEngine(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry) noexcept
    : dispatcher_(&dispatcher),
      registry_(&registry),
      pending_(),
      parents_(),
      inheritanceSubscriptions_(),
      inheritanceChangedHandler_(
          this,
          &EffectiveValueEngine::OnInheritancePropertyChanged) {}

void EffectiveValueEngine::Shutdown() noexcept {
    if (phaseHook_.IsValid() && dispatcher_ != nullptr &&
        dispatcher_->CheckAccess()) {
        static_cast<void>(dispatcher_->RemoveFrameHook(phaseHook_));
        phaseHook_ = {};
    }

    for (DependencyObject* object : inheritanceSubscriptions_) {
        if (object == nullptr) continue;
        for (const DependencyProperty& property : registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(object->RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object->RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
    }
    inheritanceSubscriptions_.Clear();
    pending_.Clear();
    parents_.Clear();
}

EffectiveValueEngine::~EffectiveValueEngine() noexcept {
    Shutdown();
}

Base::Result<void> EffectiveValueEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (phaseHook_.IsValid()) return {};
    if (!registry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyPropertyRegistry must be frozen before the value engine");
    }

    Base::Result<DispatcherFrameHookHandle> hook =
        dispatcher_->RegisterFrameHook(
            ::Aero::Threading::DispatcherFramePhase::PropertyChanges,
            &EffectiveValueEngine::PropertyChangesHook,
            this,
            nullptr);
    if (!hook) return hook.GetStatus();
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> EffectiveValueEngine::VerifyMutable() const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!phaseHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "EffectiveValueEngine is not initialized");
    }
    if (flushing_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Provider mutation is not allowed while values are being flushed");
    }
    return {};
}


Base::Result<void> EffectiveValueEngine::QueueObjectProperty(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    if (&object.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DependencyObject belongs to another Dispatcher");
    }
    const DependencyProperty* registered = registry_->Find(property);
    if (registered == nullptr ||
        registered->MetadataFor(object.RuntimeType()) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    property = registered->Handle();
    Base::Result<StoredValueEntry*> ensured =
        AeroGuiInternal::EnsureEntry(object, property);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry* stored = ensured.Value();
    if (stored->Queued()) return {};
    // C1: FIFO insertion order replaces the monotonic queueSequence. Skipping
    // SetQueueSequence avoids forcing a StoredValueRare allocation on every
    // enqueue for properties that otherwise need no rare block.
    stored->SetQueued(true);
    Pending pending;
    pending.object = &object;
    pending.property = property;
    pending.queueSequence = 0U;
    Base::Result<void> queued = pending_.PushBack(pending);
    if (!queued) {
        stored->SetQueued(false);
        return queued.GetStatus();
    }
    return {};
}

DependencyObject* EffectiveValueEngine::InheritanceParent(
    const DependencyObject& child) const noexcept {
    DependencyObject* key = const_cast<DependencyObject*>(&child);
    DependencyObject* const* parent = parents_.Find(key);
    return parent != nullptr ? *parent : nullptr;
}

Base::Result<void> EffectiveValueEngine::SetInheritanceParent(
    DependencyObject& child,
    DependencyObject* parent) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) return ready.GetStatus();
    if (&child.GetDispatcher() != dispatcher_ ||
        (parent != nullptr &&
         &parent->GetDispatcher() != dispatcher_)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Inheritance objects must belong to the value engine Dispatcher");
    }
    if (parent == &child) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "An object cannot inherit from itself");
    }

    DependencyObject* cursor = parent;
    while (cursor != nullptr) {
        if (cursor == &child) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Inheritance parent assignment would create a cycle");
        }
        cursor = InheritanceParent(*cursor);
    }

    DependencyObject* previousParent = InheritanceParent(child);
    if (parent != nullptr) {
        Base::Result<void> subscribed = EnsureInheritanceSubscription(child);
        if (!subscribed) return subscribed.GetStatus();
        subscribed = EnsureInheritanceSubscription(*parent);
        if (!subscribed) return subscribed.GetStatus();
        auto stored = parents_.Set(&child, parent);
        if (!stored) return stored.GetStatus();
    } else {
        parents_.Erase(&child);
        Base::Result<void> subscribed = EnsureInheritanceSubscription(child);
        if (!subscribed) return subscribed.GetStatus();
    }

    const auto participates = [this](DependencyObject& object) noexcept {
        if (parents_.Find(&object) != nullptr) return true;
        for (auto& link : parents_) {
            if (link.Value() == &object) return true;
        }
        return false;
    };

    if (parent != nullptr && !participates(child)) {
        RemoveInheritanceSubscription(child);
    }
    if (previousParent != nullptr &&
        previousParent != parent &&
        !participates(*previousParent)) {
        RemoveInheritanceSubscription(*previousParent);
    }

    Base::Vector<MemberId> keys;
    AeroGuiInternal::ForEachStoredKey(
        child,
        [](void* context, MemberId key) noexcept {
            static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
        },
        &keys);
    for (MemberId key : keys) {
        Base::Result<void> queued =
            QueueObjectProperty(child, DependencyPropertyHandle{key});
        if (!queued) return queued.GetStatus();
    }
    return {};
}

Base::Result<void> EffectiveValueEngine::QueueDescendants(
    DependencyObject& parent,
    DependencyPropertyHandle property) noexcept {
    Base::Vector<DependencyObject*> frontier;
    Base::Result<void> root = frontier.PushBack(&parent);
    if (!root) return root.GetStatus();
    const DependencyProperty* registered = registry_->Find(property);
    std::uint32_t cursor = 0U;
    while (cursor < frontier.Size()) {
        DependencyObject* current = frontier[cursor++];
        auto enqueue = [&](DependencyObject* child) -> Base::Result<void> {
            if (child == nullptr || child == current) {
                return {};
            }
            for (DependencyObject* seen : frontier) {
                if (seen == child) {
                    return {};
                }
            }
            const PropertyMetadata* metadata = registered != nullptr
                ? registered->MetadataFor(child->RuntimeType())
                : nullptr;
            const bool inherits = metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits);
            if (inherits ||
                AeroGuiInternal::FindEntry(*child, property) != nullptr) {
                Base::Result<void> queued =
                    QueueObjectProperty(*child, property);
                if (!queued) return queued.GetStatus();
            }
            return frontier.PushBack(child);
        };

        Base::Vector<DependencyObject*> children;
        for (auto& link : parents_) {
            if (link.Value() != current || link.Key() == nullptr) continue;
            Base::Result<void> stored = children.PushBack(link.Key());
            if (!stored) return stored.GetStatus();
        }

        if (::Aero::Media::Visual* visual =
                ::Aero::TryCast<::Aero::Media::Visual>(current)) {
            const std::uint32_t visualCount =
                ::Aero::Media::VisualTreeHelper::GetChildrenCount(*visual);
            for (std::uint32_t index = 0U; index < visualCount; ++index) {
                ::Aero::Media::Visual* childVisual =
                    ::Aero::Media::VisualTreeHelper::GetChild(*visual, index);
                if (childVisual == nullptr) continue;
                Base::Result<void> stored = children.PushBack(childVisual);
                if (!stored) return stored.GetStatus();
            }
        }

        const std::uint32_t logicalCount =
            ::Aero::LogicalTreeHelper::GetChildrenCount(*current);
        for (std::uint32_t index = 0U; index < logicalCount; ++index) {
            DependencyObject* child =
                ::Aero::LogicalTreeHelper::GetChild(*current, index);
            if (child == nullptr) continue;
            Base::Result<void> stored = children.PushBack(child);
            if (!stored) return stored.GetStatus();
        }

        for (DependencyObject* child : children) {
            Base::Result<void> queued = enqueue(child);
            if (!queued) return queued.GetStatus();
        }
    }
    return {};
}

Base::Result<void>
EffectiveValueEngine::EnsureInheritanceSubscription(
    DependencyObject& object) noexcept {
    for (DependencyObject* subscribed : inheritanceSubscriptions_) {
        if (subscribed == &object) return {};
    }

    for (const DependencyProperty& property : registry_->Properties()) {
        const PropertyMetadata* metadata =
            property.MetadataFor(object.RuntimeType());
        if (metadata == nullptr ||
            !HasFlag(
                metadata->flags,
                PropertyMetadataFlags::Inherits)) {
            continue;
        }

        Base::Result<void> added =
            object.AddValueChangedHandlerChecked(
                property.Handle(),
                inheritanceChangedHandler_);
        if (!added) {
            for (const DependencyProperty& rollback :
                 registry_->Properties()) {
                if (rollback.Handle() == property.Handle()) break;
                const PropertyMetadata* rollbackMetadata =
                    rollback.MetadataFor(object.RuntimeType());
                if (rollbackMetadata != nullptr &&
                    HasFlag(
                        rollbackMetadata->flags,
                        PropertyMetadataFlags::Inherits)) {
                    static_cast<void>(
                        object.RemoveValueChangedHandler(
                            rollback.Handle(),
                            inheritanceChangedHandler_));
                }
            }
            return added.GetStatus();
        }

        Base::Result<void> queued =
            QueueObjectProperty(object, property.Handle());
        if (!queued) {
            static_cast<void>(object.RemoveValueChangedHandler(
                property.Handle(),
                inheritanceChangedHandler_));
            return queued.GetStatus();
        }
    }

    Base::Result<void> retained =
        inheritanceSubscriptions_.PushBack(&object);
    if (!retained) {
        for (const DependencyProperty& property : registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(object.RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object.RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
        return retained.GetStatus();
    }
    return {};
}

void EffectiveValueEngine::RemoveInheritanceSubscription(
    DependencyObject& object) noexcept {
    for (std::uint32_t index = 0U;
         index < inheritanceSubscriptions_.Size();
         ++index) {
        if (inheritanceSubscriptions_[index] != &object) continue;
        for (const DependencyProperty& property : registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(object.RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object.RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
        for (std::uint32_t next = index + 1U;
             next < inheritanceSubscriptions_.Size();
             ++next) {
            inheritanceSubscriptions_[next - 1U] =
                inheritanceSubscriptions_[next];
        }
        inheritanceSubscriptions_.PopBack();
        return;
    }
}

void EffectiveValueEngine::OnInheritancePropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const DependencyProperty* property = registry_->Find(args.GetProperty());
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(object.RuntimeType())
        : nullptr;
    if (metadata == nullptr ||
        !HasFlag(
            metadata->flags,
            PropertyMetadataFlags::Inherits)) {
        return;
    }
    static_cast<void>(QueueDescendants(object, args.GetProperty()));
}


Base::Result<void> EffectiveValueEngine::Apply(Pending& entry) noexcept {
    const DependencyProperty* property = registry_->Find(entry.property);
    if (property == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property is no longer registered");
    const PropertyMetadata* metadata = property->MetadataFor(entry.object->RuntimeType());
    if (metadata == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property metadata is unavailable for the object");
    PropertyValue inheritedValue;
    const PropertyValue* inherited = nullptr;
    if (HasFlag(metadata->flags, PropertyMetadataFlags::Inherits)) {
        DependencyObject* inheritFrom = InheritanceParent(*entry.object);
        if (inheritFrom == nullptr) {
            if (::Aero::Media::Visual* visual =
                    ::Aero::TryCast<::Aero::Media::Visual>(entry.object)) {
                inheritFrom = ::Aero::Media::VisualTreeHelper::GetParent(*visual);
            }
        }
        if (inheritFrom == nullptr) {
            inheritFrom = ::Aero::LogicalTreeHelper::GetParent(*entry.object);
        }
        while (inheritFrom != nullptr &&
            property->MetadataFor(inheritFrom->RuntimeType()) == nullptr) {
            DependencyObject* next = InheritanceParent(*inheritFrom);
            if (next == nullptr) {
                if (::Aero::Media::Visual* visual =
                        ::Aero::TryCast<::Aero::Media::Visual>(inheritFrom)) {
                    next = ::Aero::Media::VisualTreeHelper::GetParent(*visual);
                }
            }
            if (next == nullptr) {
                next = ::Aero::LogicalTreeHelper::GetParent(*inheritFrom);
            }
            inheritFrom = next;
        }
        if (inheritFrom != nullptr) {
            PropertyValue value = inheritFrom->GetValue(entry.property);
            inheritedValue = std::move(value);
            inherited = &inheritedValue;
        }
    }
    Base::Result<void> stored = AeroGuiInternal::ApplyInheritedValue(*entry.object, entry.property, inherited);
    if (!stored) return stored.GetStatus();
    return AeroGuiInternal::RecomputeEffectiveValue(*entry.object, entry.property);
}

Base::Result<std::uint32_t> EffectiveValueEngine::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!phaseHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "EffectiveValueEngine is not initialized");
    }
    if (flushing_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Effective value flushing is already active");
    }

    FlushScope scope(flushing_);
    std::uint32_t processed = 0U;
    // C1: FIFO head-index drain. pending_ is appended in queueSequence order
    // and QueueObjectProperty refuses duplicates while Queued(), so the vector
    // is already sequence-ordered; the former min-sequence scan was O(n^2).
    // Newly queued entries appended by Apply() during the flush are processed
    // in the same pass, matching the previous while(true) semantics.
    std::uint32_t head = 0U;
    auto compactPrefix = [&](std::uint32_t keepFrom) noexcept {
        const std::uint32_t tail = pending_.Size();
        std::uint32_t write = 0U;
        for (std::uint32_t read = keepFrom; read < tail; ++read) {
            pending_[write++] = std::move(pending_[read]);
        }
        while (pending_.Size() > write) {
            pending_.PopBack();
        }
    };

    while (head < pending_.Size()) {
        Pending entry = pending_[head];
        ++head;
        if (entry.object == nullptr) {
            continue;
        }
        StoredValueEntry* stored =
            AeroGuiInternal::FindEntry(*entry.object, entry.property);
        if (stored == nullptr || !stored->Queued()) {
            continue;
        }
        stored->SetQueued(false);
        Base::Result<void> applied = Apply(entry);
        if (!applied) {
            stored->SetQueued(true);
            // Keep the failed entry plus the unprocessed tail.
            compactPrefix(head - 1U);
            return applied.GetStatus();
        }
        ++processed;
        if (processed >= 65536U) {
            break;
        }
    }
    compactPrefix(head);
    return processed;
}

Base::Result<EffectiveValueDiagnostics> EffectiveValueEngine::Diagnostics(
    const DependencyObject& object, DependencyPropertyHandle property) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    return object.GetValueSourceInfo(property);
}

bool EffectiveValueEngine::IsMutableBaseRank(
    PropertyValueRank rank) noexcept {
    switch (rank) {
    case PropertyValueRank::ThemeStyleSetter:
    case PropertyValueRank::ThemeStyleTrigger:
    case PropertyValueRank::StyleSetter:
    case PropertyValueRank::TemplateTrigger:
    case PropertyValueRank::StyleTrigger:
    case PropertyValueRank::ImplicitStyle:
    case PropertyValueRank::TemplatedParentSetter:
    case PropertyValueRank::TemplatedParentTrigger:
    case PropertyValueRank::VisualState:
        return true;
    default:
        return false;
    }
}

Base::Result<void> EffectiveValueEngine::SetProviderContribution(
    DependencyObject& object, DependencyPropertyHandle property,
    PropertyProviderToken token, const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (!token.IsValid() || !IsMutableBaseRank(token.rank)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property provider contribution is invalid");
    }
    Base::Result<void> queued = QueueObjectProperty(object, property);
    if (!queued) return queued.GetStatus();
    return AeroGuiInternal::ApplyProviderContribution(object, property, token, value);
}

Base::Result<bool> EffectiveValueEngine::ClearProviderContribution(
    DependencyObject& object, DependencyPropertyHandle property,
    PropertyProviderToken token) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (!token.IsValid() || !IsMutableBaseRank(token.rank)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property provider contribution is invalid");
    }
    if (AeroGuiInternal::FindEntry(object, property) == nullptr) return false;
    Base::Result<bool> cleared = AeroGuiInternal::ClearProviderContribution(object, property, token);
    if (!cleared || !cleared.Value()) return cleared;
    Base::Result<void> queued = QueueObjectProperty(object, property);
    if (!queued) return queued.GetStatus();
    return true;
}

Base::Result<std::uint32_t> EffectiveValueEngine::ClearProviderOrigin(
    DependencyObject& object, std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (origin == 0U) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property provider origin must be nonzero");
    std::uint32_t removed = 0U;
    Base::Vector<MemberId> keys;
    AeroGuiInternal::ForEachStoredKey(
        object,
        [](void* context, MemberId key) noexcept {
            static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
        },
        &keys);
    for (MemberId key : keys) {
        DependencyPropertyHandle property{key};
        Base::Result<bool> cleared =
            AeroGuiInternal::ClearProviderOrigin(object, property, origin);
        if (!cleared) return cleared.GetStatus();
        if (!cleared.Value()) continue;
        ++removed;
        Base::Result<void> queued = QueueObjectProperty(object, property);
        if (!queued) return queued.GetStatus();
    }
    return removed;
}

Base::Result<void> EffectiveValueEngine::SetLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<void> queued = QueueObjectProperty(object, property);
    if (!queued) return queued.GetStatus();
    return AeroGuiInternal::ApplyLocalExpression(object, property, expression);
}

Base::Result<void> EffectiveValueEngine::ClearLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (AeroGuiInternal::FindEntry(object, property) == nullptr) return {};
    Base::Result<bool> cleared = AeroGuiInternal::ClearLocalExpression(object, property);
    if (!cleared) return cleared.GetStatus();
    if (!cleared.Value()) return {};
    return QueueObjectProperty(object, property);
}

Base::Result<void> EffectiveValueEngine::SetAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<void> queued = QueueObjectProperty(object, property);
    if (!queued) return queued.GetStatus();
    return AeroGuiInternal::ApplyAnimationValue(object, property, value);
}

Base::Result<void> EffectiveValueEngine::ClearAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (AeroGuiInternal::FindEntry(object, property) == nullptr) return {};
    Base::Result<bool> cleared = AeroGuiInternal::ClearAnimationValue(object, property);
    if (!cleared) return cleared.GetStatus();
    if (!cleared.Value()) return {};
    return QueueObjectProperty(object, property);
}

Base::Result<void> EffectiveValueEngine::Invalidate(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) return ready.GetStatus();
    Base::Result<bool> invalidated = AeroGuiInternal::InvalidateBaseValue(object, property);
    if (!invalidated) return invalidated.GetStatus();
    Base::Result<void> queued = QueueObjectProperty(object, property);
    if (!queued) return queued.GetStatus();
    return QueueDescendants(object, property);
}

Base::Result<void> EffectiveValueEngine::DetachObject(DependencyObject& object) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Vector<MemberId> keys;
    AeroGuiInternal::ForEachStoredKey(
        object,
        [](void* context, MemberId key) noexcept {
            static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
        },
        &keys);
    for (MemberId key : keys) {
        Base::Result<void> cleared =
            AeroGuiInternal::DropEngineValueState(object, DependencyPropertyHandle{key});
        if (!cleared) return cleared.GetStatus();
    }
    std::uint32_t pendingIndex = 0U;
    while (pendingIndex < pending_.Size()) {
        if (pending_[pendingIndex].object == &object) {
            for (std::uint32_t current = pendingIndex + 1U;
                 current < pending_.Size(); ++current) {
                pending_[current - 1U] = std::move(pending_[current]);
            }
            pending_.PopBack();
        } else {
            ++pendingIndex;
        }
    }
    parents_.Erase(&object);
    Base::Vector<DependencyObject*> children;
    for (auto& link : parents_) {
        if (link.Value() == &object) {
            children.PushBack(link.Key());
        }
    }
    for (DependencyObject* child : children) {
        parents_.Erase(child);
    }
    RemoveInheritanceSubscription(object);
    return {};
}

std::uint32_t EffectiveValueEngine::PendingPropertyCount() const noexcept {
    std::uint32_t count = 0U;
    for (const Pending& entry : pending_) {
        const StoredValueEntry* stored = entry.object != nullptr
            ? AeroGuiInternal::FindEntry(*entry.object, entry.property)
            : nullptr;
        if (stored != nullptr && stored->Queued()) ++count;
    }
    return count;
}

void EffectiveValueEngine::PropertyChangesHook(
    void* context) noexcept {
    auto* engine = static_cast<EffectiveValueEngine*>(context);
    static_cast<void>(engine->Flush());
}

} // namespace Aero::Meta

// Consolidated implementation. Keep sections ordered by dependency.

// ===== DependencyProperty =====

#include <Aero/Gui/DependencyProperty.hpp>
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>

#include <cstdio>
#include <cstdint>
#include <limits>
#include <utility>

namespace Aero::GuiPrivate::Detail {

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
        } else if (metadata != nullptr && value.AsObject() &&
                   metadata->Types().IsAssignableFrom(
                       targetType, value.AsObject()->RuntimeType())) {
            value = Meta::PropertyValue::FromObject(
                targetType,
                Base::Ref<Base::Object>::FromBorrowed(*value.AsObject()));
        }
    }

    if (value.Type() != targetType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Expression value cannot be assigned to the dependency property");
    }
    return value;
}

} // namespace Aero::GuiPrivate::Detail

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

    TypeId current = forType;
    std::uint32_t remaining = typeRegistry_->TypeCount() + 1U;
    while (current != InvalidTypeId && remaining > 0U) {
        const MetadataEntry* exact = FindMetadataExact(current);
        if (exact != nullptr) {
            return &exact->metadata;
        }

        const TypeInfo* type = typeRegistry_->FindType(current);
        if (type == nullptr) {
            return nullptr;
        }
        current = type->BaseType();
        --remaining;
    }
    if (IsAttached()) {
        const MetadataEntry* owner = FindMetadataExact(registeredOwnerType_);
        return owner != nullptr ? &owner->metadata : nullptr;
    }
    return nullptr;
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
    const PropertyMetadata& metadata) noexcept {
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
    metaProperty.flags = ToTypeRegistryFlags(property.Flags(), metadata.flags);
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
      values_(),
      updateStack_(),
      changeHandlers_() {}

DependencyObject::~DependencyObject() {
    for (EffectiveValueEntry& entry : values_) {
        Impl::CommitConsumerChange(
            *this,
            entry.property,
            entry.effectiveValue,
            PropertyValue::Unset());
        ReleaseExpression(entry);
    }
}

Base::Result<void> DependencyObject::VerifyReady() const noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!objectServicesAvailable_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyObject was created without an ObjectFactoryScope");
    }
    if (registry_ == nullptr || !registry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency property registry is not frozen");
    }
    if (runtimeType_ == InvalidTypeId ||
        registry_->Types().FindType(runtimeType_) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DependencyObject runtime type is not registered");
    }
    return {};
}

std::uint32_t DependencyObject::FindEntryIndex(
    DependencyPropertyHandle property) const noexcept {
    for (std::uint32_t index = 0U; index < values_.Size(); ++index) {
        if (values_[index].property == property) {
            return index;
        }
    }
    return InvalidIndex;
}

PropertyValue DependencyObject::GetValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyValue::Unset();
    }

    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) {
        return PropertyValue::Unset();
    }
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) {
        return PropertyValue::Unset();
    }

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex
        ? values_[index].effectiveValue
        : metadata->defaultValue;
}

PropertyValue DependencyObject::ReadLocalValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyValue::Unset();
    }

    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr) {
        return PropertyValue::Unset();
    }

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex || !values_[index].hasLocal) {
        return PropertyValue::Unset();
    }
    return values_[index].localValue;
}

EffectiveValueSource DependencyObject::GetValueSource(
    DependencyPropertyHandle propertyHandle) const noexcept {
    const PropertyValueSourceInfo source = GetValueSourceInfo(propertyHandle);
    return ToLegacySource(source);
}


PropertyValueSourceInfo DependencyObject::GetValueSourceInfo(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return {};
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr)
        return {};
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex ? values_[index].sourceInfo : PropertyValueSourceInfo{};
}

void DependencyObject::SetValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetValueChecked(property, value));
}

Base::Result<void> DependencyObject::SetValueChecked(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetLocal, &value);
}

void DependencyObject::SetValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetValueChecked(key, value));
}

Base::Result<void> DependencyObject::SetValueChecked(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetLocal, &value);
}

void DependencyObject::SetCurrentValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetCurrentValueChecked(property, value));
}

Base::Result<void> DependencyObject::SetCurrentValueChecked(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetCurrent, &value);
}

void DependencyObject::SetCurrentValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetCurrentValueChecked(key, value));
}

Base::Result<void> DependencyObject::SetCurrentValueChecked(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetCurrent, &value);
}

void DependencyObject::SetReadOnlyCurrentValue(
    DependencyPropertyHandle propertyHandle,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return;
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) {
        return;
    }
    if (!property->GetIsReadOnly()) {
        return;
    }
    DependencyPropertyKey key;
    key.registry_ = registry_;
    key.property_ = propertyHandle;
    key.secret_ = property->readOnlySecret_;
    (void)ApplyChange(
        propertyHandle, &key, ChangeKind::SetLocal, &value);
}

void DependencyObject::ClearValue(
    DependencyPropertyHandle property) noexcept {
    static_cast<void>(ClearValueChecked(property));
}

Base::Result<void> DependencyObject::ClearValueChecked(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::Clear, nullptr);
}

void DependencyObject::ClearValue(
    const DependencyPropertyKey& key) noexcept {
    static_cast<void>(ClearValueChecked(key));
}

Base::Result<void> DependencyObject::ClearValueChecked(
    const DependencyPropertyKey& key) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::Clear, nullptr);
}

void DependencyObject::CoerceValue(
    DependencyPropertyHandle property) noexcept {
    static_cast<void>(CoerceValueChecked(property));
}

Base::Result<void> DependencyObject::CoerceValueChecked(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::ReCoerce, nullptr);
}

Base::Result<void> DependencyObject::AddValueChangedHandlerChecked(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    if (!property.IsValid() || handler.Empty() ||
        registry_->Find(property) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property change handler registration is invalid");
    }
    ChangeHandlerRecord record;
    record.property = property;
    record.handler = handler;
    record.active = true;
    return changeHandlers_.PushBack(std::move(record));
}

void DependencyObject::AddValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> added =
        AddValueChangedHandlerChecked(property, handler);
    if (!added) {
        Base::ReportOutOfMemory(
            sizeof(DependencyPropertyChangedEventHandler),
            alignof(DependencyPropertyChangedEventHandler),
            Base::MemoryTag::General);
    }
}

bool DependencyObject::RemoveValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return false;
    }
    if (!property.IsValid() || handler.Empty()) {
        return false;
    }
    for (std::uint32_t count = changeHandlers_.Size(); count > 0U; --count) {
        const std::uint32_t index = count - 1U;
        ChangeHandlerRecord& record = changeHandlers_[index];
        if (record.property != property || record.handler != handler ||
            !record.active) {
            continue;
        }
        if (changeHandlerNotificationDepth_ != 0U) {
            record.active = false;
        } else {
            RemoveChangeHandler(index);
        }
        return true;
    }
    return false;
}

PropertyInvalidationFlags DependencyObject::TakeInvalidations() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyInvalidationFlags::None;
    }
    const PropertyInvalidationFlags result = invalidations_;
    invalidations_ = PropertyInvalidationFlags::None;
    return result;
}

Base::Result<DependencyObject::MutationScope>
DependencyObject::BeginMutation(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) {
        return writable.GetStatus();
    }
    for (MemberId active : updateStack_) {
        if (active == property.value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Recursive mutation of the same dependency property is not allowed");
        }
    }

    Base::Result<void> pushed = updateStack_.PushBack(property.value);
    if (!pushed) {
        return pushed.GetStatus();
    }

    Base::Result<DispatcherReentrancyGuard> guard =
        GetDispatcher().EnterReentrancyGuard();
    if (!guard) {
        updateStack_.PopBack();
        return guard.GetStatus();
    }

    return MutationScope(this, std::move(guard).Value());
}

void DependencyObject::LeaveMutation() noexcept {
    AERO_ASSERT(!updateStack_.Empty());
    updateStack_.PopBack();
}


Base::Result<std::uint32_t> DependencyObject::EnsureEffectiveEntry(
    DependencyPropertyHandle propertyHandle) noexcept {
    const std::uint32_t existing = FindEntryIndex(propertyHandle);
    if (existing != InvalidIndex) return existing;
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(runtimeType_) : nullptr;
    if (property == nullptr || metadata == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    if (values_.Size() == UINT32_MAX) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "DependencyObject sparse value table limit reached");
    }
    EffectiveValueEntry entry;
    entry.property = propertyHandle;
    entry.baseValue = metadata->defaultValue;
    entry.effectiveValue = metadata->defaultValue;
    Base::Result<void> appended = values_.PushBack(std::move(entry));
    if (!appended) return appended.GetStatus();
    return values_.Size() - 1U;
}

EffectiveValueSource DependencyObject::ToLegacySource(
    const PropertyValueSourceInfo& source) noexcept {
    if (source.isCurrentValue) return EffectiveValueSource::Current;
    if (source.rank == PropertyValueRank::Default) return EffectiveValueSource::Default;
    if (source.rank == PropertyValueRank::Local ||
        source.rank == PropertyValueRank::LocalExpression) return EffectiveValueSource::Local;
    return EffectiveValueSource::Current;
}

void DependencyObject::ReleaseExpression(EffectiveValueEntry& entry) noexcept {
    if (!entry.hasExpression) return;
    const PropertyExpression expression = entry.localExpression;
    entry.localExpression = {};
    entry.hasExpression = false;
    if (expression.cleanup != nullptr) expression.cleanup(expression.context);
}

Base::Result<void> DependencyObject::ApplyProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    Base::Result<void> valid = registry_->ValidateValueFor(property, runtimeType_, value);
    if (!valid) return valid.GetStatus();
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    EffectiveValueEntry& entry = values_[ensured.Value()];
    entry.currentValue = PropertyValue::Unset();
    entry.hasCurrent = false;
    if (!entry.baseProviders.Set(token, value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "A property contribution requires a valid token and value");
    }
    return {};
}

Base::Result<bool> DependencyObject::ClearProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex) return false;
    const bool removed = values_[index].baseProviders.Remove(token);
    if (removed) { values_[index].currentValue = PropertyValue::Unset(); values_[index].hasCurrent = false; }
    return removed;
}

Base::Result<bool> DependencyObject::ClearProviderOriginInternal(
    DependencyPropertyHandle property, std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex) return false;
    const bool removed = values_[index].baseProviders.RemoveOrigin(origin) != 0U;
    if (removed) { values_[index].currentValue = PropertyValue::Unset(); values_[index].hasCurrent = false; }
    return removed;
}

Base::Result<void> DependencyObject::ApplyLocalExpressionInternal(
    DependencyPropertyHandle property, const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    if (!expression.IsValid()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property expression requires an evaluate callback");
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    EffectiveValueEntry& entry = values_[ensured.Value()];
    PropertyExpression old;
    const bool hadOld = entry.hasExpression;
    if (hadOld) old = entry.localExpression;
    entry.localExpression = expression;
    entry.hasExpression = true;
    entry.localValue = PropertyValue::Unset();
    entry.hasLocal = false;
    entry.currentValue = PropertyValue::Unset();
    entry.hasCurrent = false;
    if (hadOld && old.cleanup != nullptr) old.cleanup(old.context);
    return {};
}

Base::Result<bool> DependencyObject::ClearLocalExpressionInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasExpression) return false;
    ReleaseExpression(values_[index]);
    values_[index].currentValue = PropertyValue::Unset();
    values_[index].hasCurrent = false;
    return true;
}

Base::Result<bool> DependencyObject::InvalidateBaseValueInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasCurrent) return false;
    values_[index].currentValue = PropertyValue::Unset();
    values_[index].hasCurrent = false;
    return true;
}

Base::Result<void> DependencyObject::ApplyAnimationValueInternal(
    DependencyPropertyHandle property, const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    if (value.IsUnset()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "Animation values cannot be Unset");
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    values_[ensured.Value()].animationValue = value;
    values_[ensured.Value()].hasAnimation = true;
    return {};
}

Base::Result<bool> DependencyObject::ClearAnimationValueInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasAnimation) return false;
    values_[index].animationValue = PropertyValue::Unset();
    values_[index].hasAnimation = false;
    return true;
}

Base::Result<void> DependencyObject::ApplyInheritedValueInternal(
    DependencyPropertyHandle property, const PropertyValue* value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex && value == nullptr) return {};
    if (index == InvalidIndex) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    EffectiveValueEntry& entry = values_[index];
    const bool changed = value == nullptr ? entry.hasInherited
        : (!entry.hasInherited || entry.inheritedValue != *value);
    if (value == nullptr) { entry.inheritedValue = PropertyValue::Unset(); entry.hasInherited = false; }
    else { entry.inheritedValue = *value; entry.hasInherited = true; }
    if (changed) { entry.currentValue = PropertyValue::Unset(); entry.hasCurrent = false; }
    return {};
}

Base::Result<void> DependencyObject::RecomputeEffectiveValueCore(
    DependencyPropertyHandle propertyHandle,
    const Meta::DependencyProperty& property,
    const PropertyMetadata& metadata, const PropertyValue& oldEffective,
    const PropertyValueSourceInfo& oldSourceInfo) noexcept {
    std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    const EffectiveValueEntry& stored = values_[index];
    const bool hasExpression = stored.hasExpression;
    const PropertyExpression expression = stored.localExpression;
    const bool hasLocal = stored.hasLocal;
    const PropertyValue localValue = stored.localValue;
    const bool hasCurrent = stored.hasCurrent;
    const PropertyValue currentValue = stored.currentValue;
    const bool hasInherited = stored.hasInherited;
    const PropertyValue inheritedValue = stored.inheritedValue;
    const bool hasAnimation = stored.hasAnimation;
    const PropertyValue animationValue = stored.animationValue;
    bool hasProvider = false;
    PropertyProviderToken providerToken;
    PropertyValue providerValue;
    const PropertyProviderContribution* provider = stored.baseProviders.Winner();
    if (provider != nullptr) { hasProvider = true; providerToken = provider->token; providerValue = provider->value; }

    PropertyValue baseValue;
    PropertyValueSourceInfo source;
    if (hasExpression) {
        Base::Result<PropertyValue> evaluated = expression.evaluate(
            expression.context, *this, propertyHandle);
        if (!evaluated) return evaluated.GetStatus();
        if (evaluated.Value().IsUnset()) return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed, "A property expression returned Unset");
        baseValue = std::move(evaluated).Value();
        source.rank = PropertyValueRank::LocalExpression;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 1U};
        source.expressionKind = expression.kind;
        source.hasExpression = true;
    } else if (hasLocal) {
        baseValue = localValue;
        source.rank = PropertyValueRank::Local;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 0U};
    } else if (hasProvider) {
        baseValue = providerValue;
        source.rank = providerToken.rank;
        source.token = providerToken;
    } else if (hasInherited) {
        baseValue = inheritedValue;
        source.rank = PropertyValueRank::Inherited;
        source.isInherited = true;
    } else {
        baseValue = metadata.defaultValue;
        source.rank = PropertyValueRank::Default;
    }
    if (hasCurrent) { baseValue = currentValue; source.isCurrentValue = true; }
    PropertyValue candidate = hasAnimation ? animationValue : baseValue;
    if (hasAnimation) {
        source.rank = PropertyValueRank::Animation;
        source.token = {PropertyValueRank::Animation, AnimationValueProviderOrigin, 0U};
        source.isAnimated = true;
    }
    Base::Result<PropertyValue> evaluated = registry_->EvaluateValue(
        *this, property, metadata, candidate);
    if (!evaluated) return evaluated.GetStatus();
    PropertyValue newEffective = std::move(evaluated).Value();
    source.isCoerced = newEffective != candidate;
    if (nextValueRevision_ == UINT64_MAX) return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, "Dependency property value revision limit reached");
    if (newEffective != oldEffective) {
        Base::Result<void> consumerPrepared =
            Impl::PrepareConsumerChange(
                *this,
                propertyHandle,
                oldEffective,
                newEffective);
        if (!consumerPrepared) return consumerPrepared.GetStatus();
    }
    source.revision = nextValueRevision_++;
    index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) return Base::Status::Failure(Base::ErrorCode::InternalError,
        "Dependency property entry disappeared during evaluation");
    EffectiveValueEntry& entry = values_[index];
    entry.baseValue = baseValue;
    entry.effectiveValue = newEffective;
    entry.sourceInfo = source;
    if (newEffective != oldEffective) {
        Impl::CommitConsumerChange(
            *this,
            propertyHandle,
            oldEffective,
            newEffective);
    }
    const EffectiveValueSource oldSource = ToLegacySource(oldSourceInfo);
    const EffectiveValueSource newSource = ToLegacySource(source);
    if (newEffective != oldEffective) {
        const PropertyInvalidationFlags flags = AccumulateInvalidations(metadata.flags);
        const DependencyPropertyChangedEventArgs args{
            propertyHandle,
            oldEffective,
            newEffective,
            oldSource,
            newSource};
        if (!metadata.changed.Empty()) metadata.changed(*this, args);
        NotifyValueChanged(args);
        OnPropertyInvalidated(flags);
    }
    index = FindEntryIndex(propertyHandle);
    if (index != InvalidIndex) {
        const EffectiveValueEntry& finalEntry = values_[index];
        const bool shouldStore = finalEntry.hasLocal || finalEntry.hasCurrent ||
            finalEntry.hasExpression || finalEntry.hasInherited || finalEntry.hasAnimation ||
            !finalEntry.baseProviders.GetIsEmpty() ||
            finalEntry.effectiveValue != metadata.defaultValue;
        if (!shouldStore) RemoveEntry(index);
    }
    return {};
}

Base::Result<void> DependencyObject::RecomputeEffectiveValueInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(runtimeType_) : nullptr;
    if (property == nullptr || metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    Base::Result<MutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    MutationScope mutation = std::move(mutationResult).Value();
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    const PropertyValue oldEffective = index != InvalidIndex
        ? values_[index].effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = index != InvalidIndex
        ? values_[index].sourceInfo : PropertyValueSourceInfo{};
    return RecomputeEffectiveValueCore(propertyHandle, *property, *metadata,
        oldEffective, oldSourceInfo);
}

Base::Result<void> DependencyObject::DropEngineValueStateInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) return {};
    EffectiveValueEntry& entry = values_[index];
    entry.baseProviders.Clear();
    ReleaseExpression(entry);
    entry.inheritedValue = PropertyValue::Unset();
    entry.animationValue = PropertyValue::Unset();
    entry.currentValue = PropertyValue::Unset();
    entry.hasInherited = false;
    entry.hasAnimation = false;
    entry.hasCurrent = false;
    return RecomputeEffectiveValueInternal(propertyHandle);
}


Base::Result<void> DependencyObject::ApplyChange(
    DependencyPropertyHandle propertyHandle, const DependencyPropertyKey* key,
    ChangeKind kind, const PropertyValue* requestedValue) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property was not found");
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    if (kind != ChangeKind::ReCoerce && property->GetIsReadOnly() &&
        !registry_->ValidateKey(propertyHandle, key)) return ReadOnlyStatus();
    if ((kind == ChangeKind::SetLocal || kind == ChangeKind::SetCurrent) &&
        requestedValue == nullptr) return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property set operation requires a value");
    Base::Result<MutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    MutationScope mutation = std::move(mutationResult).Value();
    std::uint32_t index = FindEntryIndex(propertyHandle);
    const bool hadEntry = index != InvalidIndex;
    if (!hadEntry && kind == ChangeKind::Clear) return {};
    if (!hadEntry) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    EffectiveValueEntry& entry = values_[index];
    const PropertyValue oldEffective = hadEntry ? entry.effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = hadEntry
        ? entry.sourceInfo : PropertyValueSourceInfo{};
    const std::uint64_t oldRevision = oldSourceInfo.revision;
    const PropertyValue oldLocal = entry.localValue;
    const PropertyValue oldCurrent = entry.currentValue;
    const PropertyExpression oldExpression = entry.localExpression;
    const bool oldHasLocal = entry.hasLocal;
    const bool oldHasCurrent = entry.hasCurrent;
    const bool oldHasExpression = entry.hasExpression;
    const bool removesExpression = oldHasExpression &&
        (kind == ChangeKind::SetLocal || kind == ChangeKind::Clear);
    switch (kind) {
    case ChangeKind::SetLocal:
        entry.localExpression = {}; entry.hasExpression = false;
        entry.localValue = *requestedValue; entry.hasLocal = true;
        entry.currentValue = PropertyValue::Unset(); entry.hasCurrent = false; break;
    case ChangeKind::SetCurrent:
        entry.currentValue = *requestedValue; entry.hasCurrent = true; break;
    case ChangeKind::Clear:
        entry.localExpression = {}; entry.hasExpression = false;
        entry.localValue = PropertyValue::Unset(); entry.currentValue = PropertyValue::Unset();
        entry.hasLocal = false; entry.hasCurrent = false; break;
    case ChangeKind::ReCoerce: break;
    }
    Base::Result<void> recomputed = RecomputeEffectiveValueCore(
        propertyHandle, *property, *metadata, oldEffective, oldSourceInfo);
    if (recomputed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return {};
    }
    index = FindEntryIndex(propertyHandle);
    const bool committed = index != InvalidIndex &&
        values_[index].sourceInfo.revision != oldRevision;
    if (committed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return recomputed.GetStatus();
    }
    if (index != InvalidIndex) {
        values_[index].localValue = oldLocal;
        values_[index].currentValue = oldCurrent;
        values_[index].localExpression = oldExpression;
        values_[index].hasLocal = oldHasLocal;
        values_[index].hasCurrent = oldHasCurrent;
        values_[index].hasExpression = oldHasExpression;
        if (!hadEntry) RemoveEntry(index);
    }
    return recomputed.GetStatus();
}

void DependencyObject::OnPropertyInvalidated(
    PropertyInvalidationFlags) noexcept {
}

Base::Result<void> DependencyObject::VerifyMutationAllowed() const noexcept {
    return {};
}

void DependencyObject::RemoveEntry(std::uint32_t index) noexcept {
    AERO_ASSERT(index < values_.Size());
    ReleaseExpression(values_[index]);
    for (std::uint32_t current = index + 1U;
         current < values_.Size();
         ++current) {
        values_[current - 1U] = std::move(values_[current]);
    }
    values_.PopBack();
}

void DependencyObject::RemoveChangeHandler(std::uint32_t index) noexcept {
    AERO_ASSERT(index < changeHandlers_.Size());
    for (std::uint32_t current = index + 1U;
         current < changeHandlers_.Size();
         ++current) {
        changeHandlers_[current - 1U] = std::move(changeHandlers_[current]);
    }
    changeHandlers_.PopBack();
}

void DependencyObject::NotifyValueChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    AERO_ASSERT(changeHandlerNotificationDepth_ != UINT32_MAX);
    ++changeHandlerNotificationDepth_;
    const std::uint32_t snapshotCount = changeHandlers_.Size();
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        if (index >= changeHandlers_.Size()) {
            break;
        }
        ChangeHandlerRecord& record = changeHandlers_[index];
        if (record.active && record.property == args.GetProperty()) {
            record.handler(*this, args);
        }
    }
    --changeHandlerNotificationDepth_;
    if (changeHandlerNotificationDepth_ != 0U) {
        return;
    }
    for (std::uint32_t index = 0U; index < changeHandlers_.Size();) {
        if (!changeHandlers_[index].active) {
            RemoveChangeHandler(index);
        } else {
            ++index;
        }
    }
}

PropertyInvalidationFlags DependencyObject::AccumulateInvalidations(
    PropertyMetadataFlags metadataFlags) noexcept {
    PropertyInvalidationFlags change = PropertyInvalidationFlags::None;
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsMeasure)) {
        change |= PropertyInvalidationFlags::Measure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsArrange)) {
        change |= PropertyInvalidationFlags::Arrange;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsRender)) {
        change |= PropertyInvalidationFlags::Render;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::Inherits)) {
        change |= PropertyInvalidationFlags::Inheritance;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentMeasure)) {
        change |= PropertyInvalidationFlags::ParentMeasure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentArrange)) {
        change |= PropertyInvalidationFlags::ParentArrange;
    }
    invalidations_ |= change;
    return change;
}

} // namespace Aero


// ===== EffectiveValueEngine =====




namespace Aero::Meta {
namespace {

constexpr std::uint32_t EffectiveInvalidIndex = UINT32_MAX;

Base::Status InvalidProviderStatus() noexcept {
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
      entries_(),
      parents_(),
      inheritanceSubscriptions_(),
      inheritanceChangedHandler_(
          this,
          &EffectiveValueEngine::OnInheritancePropertyChanged) {}

EffectiveValueEngine::~EffectiveValueEngine() noexcept {
    if (phaseHook_.IsValid() && dispatcher_ != nullptr &&
        dispatcher_->CheckAccess()) {
        static_cast<void>(dispatcher_->RemoveFrameHook(phaseHook_));
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

std::uint32_t EffectiveValueEngine::FindEntryIndex(
    const DependencyObject& object,
    DependencyPropertyHandle property) const noexcept {
    for (std::uint32_t index = 0U;
         index < entries_.Size();
         ++index) {
        if (entries_[index].object == &object &&
            entries_[index].property == property) {
            return index;
        }
    }
    return EffectiveInvalidIndex;
}

Base::Result<std::uint32_t> EffectiveValueEngine::EnsureEntry(
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
        thread_local char message[384];
        const TypeInfo* objectType =
            registry_->Types().FindType(object.RuntimeType());
        const Base::StringView propertyName = registered != nullptr
            ? registered->Name()
            : Base::StringView("<unknown>");
        const Base::StringView typeName = objectType != nullptr
            ? objectType->Name()
            : Base::StringView("<unknown>");
        std::snprintf(
            message,
            sizeof(message),
            "Dependency property '%.*s' does not apply to object type '%.*s'",
            static_cast<int>(propertyName.SizeBytes()),
            propertyName.Data(),
            static_cast<int>(typeName.SizeBytes()),
            typeName.Data());
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            message);
    }

    const std::uint32_t existing = FindEntryIndex(object, property);
    if (existing != EffectiveInvalidIndex) return existing;
    if (entries_.Size() == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Effective value entry limit reached");
    }

    Entry entry;
    entry.object = &object;
    entry.property = property;
    Base::Result<void> appended =
        entries_.PushBack(std::move(entry));
    if (!appended) return appended.GetStatus();
    return entries_.Size() - 1U;
}

std::uint32_t EffectiveValueEngine::FindParentIndex(
    const DependencyObject& child) const noexcept {
    for (std::uint32_t index = 0U;
         index < parents_.Size();
         ++index) {
        if (parents_[index].child == &child) return index;
    }
    return EffectiveInvalidIndex;
}

DependencyObject* EffectiveValueEngine::InheritanceParent(
    const DependencyObject& child) const noexcept {
    const std::uint32_t index = FindParentIndex(child);
    return index != EffectiveInvalidIndex ? parents_[index].parent : nullptr;
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

    const std::uint32_t existing = FindParentIndex(child);
    DependencyObject* previousParent = existing != EffectiveInvalidIndex
        ? parents_[existing].parent
        : nullptr;

    if (parent != nullptr) {
        Base::Result<void> subscribed =
            EnsureInheritanceSubscription(child);
        if (!subscribed) return subscribed.GetStatus();
        subscribed = EnsureInheritanceSubscription(*parent);
        if (!subscribed) return subscribed.GetStatus();
    }

    if (parent == nullptr) {
        if (existing != EffectiveInvalidIndex) RemoveParent(existing);
    } else if (existing != EffectiveInvalidIndex) {
        parents_[existing].parent = parent;
    } else {
        Base::Result<void> appended =
            parents_.PushBack(ParentLink{&child, parent});
        if (!appended) return appended.GetStatus();
    }

    const auto participates = [this](
        const DependencyObject& object) noexcept {
        for (const ParentLink& link : parents_) {
            if (link.child == &object || link.parent == &object) {
                return true;
            }
        }
        return false;
    };

    if (!participates(child)) {
        RemoveInheritanceSubscription(child);
    }
    if (previousParent != nullptr &&
        previousParent != parent &&
        !participates(*previousParent)) {
        RemoveInheritanceSubscription(*previousParent);
    }

    for (std::uint32_t index = 0U;
         index < entries_.Size();
         ++index) {
        if (entries_[index].object == &child) {
            Base::Result<void> queued = QueueEntry(index);
            if (!queued) return queued.GetStatus();
        }
    }
    return {};
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
        return true;
    default:
        return false;
    }
}


Base::Result<void> EffectiveValueEngine::SetProviderContribution(
    DependencyObject& object, DependencyPropertyHandle property,
    PropertyProviderToken token, const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (!token.IsValid() || !IsMutableBaseRank(token.rank)) return InvalidProviderStatus();
    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) return ensured.GetStatus();
    Base::Result<void> queued = QueueEntry(ensured.Value());
    if (!queued) return queued.GetStatus();
    return object.ApplyProviderContributionInternal(property, token, value);
}

Base::Result<bool> EffectiveValueEngine::ClearProviderContribution(
    DependencyObject& object, DependencyPropertyHandle property,
    PropertyProviderToken token) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (!token.IsValid() || !IsMutableBaseRank(token.rank)) return InvalidProviderStatus();
    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == EffectiveInvalidIndex) return false;
    Base::Result<bool> cleared = object.ClearProviderContributionInternal(property, token);
    if (!cleared || !cleared.Value()) return cleared;
    Base::Result<void> queued = QueueEntry(index); if (!queued) return queued.GetStatus();
    return true;
}

Base::Result<std::uint32_t> EffectiveValueEngine::ClearProviderOrigin(
    DependencyObject& object, std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (origin == 0U) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property provider origin must be nonzero");
    std::uint32_t removed = 0U;
    for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
        Entry& entry = entries_[index]; if (entry.object != &object) continue;
        Base::Result<bool> cleared =
            object.ClearProviderOriginInternal(entry.property, origin);
        if (!cleared) return cleared.GetStatus();
        if (!cleared.Value()) continue;
        ++removed;
        Base::Result<void> queued = QueueEntry(index);
        if (!queued) return queued.GetStatus();
    }
    return removed;
}

Base::Result<void> EffectiveValueEngine::SetLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) return ensured.GetStatus();
    Base::Result<void> queued = QueueEntry(ensured.Value()); if (!queued) return queued.GetStatus();
    return object.ApplyLocalExpressionInternal(property, expression);
}

Base::Result<void> EffectiveValueEngine::ClearLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == EffectiveInvalidIndex) return {};
    Base::Result<bool> cleared =
        object.ClearLocalExpressionInternal(property);
    if (!cleared) return cleared.GetStatus();
    if (!cleared.Value()) return {};
    return QueueEntry(index);
}

Base::Result<void> EffectiveValueEngine::SetAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) return ensured.GetStatus();
    Base::Result<void> queued = QueueEntry(ensured.Value()); if (!queued) return queued.GetStatus();
    return object.ApplyAnimationValueInternal(property, value);
}

Base::Result<void> EffectiveValueEngine::ClearAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == EffectiveInvalidIndex) return {};
    Base::Result<bool> cleared =
        object.ClearAnimationValueInternal(property);
    if (!cleared) return cleared.GetStatus();
    if (!cleared.Value()) return {};
    return QueueEntry(index);
}

Base::Result<void> EffectiveValueEngine::Invalidate(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) return ready.GetStatus();
    Base::Result<std::uint32_t> ensured =
        EnsureEntry(object, property);
    if (!ensured) return ensured.GetStatus();
    Base::Result<bool> invalidated = object.InvalidateBaseValueInternal(property);
    if (!invalidated) return invalidated.GetStatus();
    Base::Result<void> queued = QueueEntry(ensured.Value());
    if (!queued) return queued.GetStatus();
    return QueueDescendants(object, property);
}

Base::Result<void> EffectiveValueEngine::QueueEntry(
    std::uint32_t index) noexcept {
    AERO_ASSERT(index < entries_.Size());
    Entry& entry = entries_[index];
    if (entry.queued) return {};
    if (nextQueueSequence_ == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Effective value queue sequence limit reached");
    }
    entry.queued = true;
    entry.queueSequence = nextQueueSequence_++;
    return {};
}

Base::Result<void> EffectiveValueEngine::QueueDescendants(
    DependencyObject& parent,
    DependencyPropertyHandle property) noexcept {
    Base::Vector<DependencyObject*> frontier;
    Base::Result<void> root = frontier.PushBack(&parent);
    if (!root) return root.GetStatus();

    std::uint32_t cursor = 0U;
    while (cursor < frontier.Size()) {
        DependencyObject* current = frontier[cursor++];
        for (const ParentLink& link : parents_) {
            if (link.parent != current || link.child == nullptr) continue;
            const std::uint32_t entryIndex =
                FindEntryIndex(*link.child, property);
            if (entryIndex != EffectiveInvalidIndex) {
                Base::Result<void> queued = QueueEntry(entryIndex);
                if (!queued) return queued.GetStatus();
            }
            Base::Result<void> pushed =
                frontier.PushBack(link.child);
            if (!pushed) return pushed.GetStatus();
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

        Base::Result<std::uint32_t> entry =
            EnsureEntry(object, property.Handle());
        if (!entry) {
            static_cast<void>(object.RemoveValueChangedHandler(
                property.Handle(),
                inheritanceChangedHandler_));
            return entry.GetStatus();
        }
        Base::Result<void> queued = QueueEntry(entry.Value());
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


Base::Result<void> EffectiveValueEngine::Apply(Entry& entry) noexcept {
    const DependencyProperty* property = registry_->Find(entry.property);
    if (property == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property is no longer registered");
    const PropertyMetadata* metadata = property->MetadataFor(entry.object->RuntimeType());
    if (metadata == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property metadata is unavailable for the object");
    PropertyValue inheritedValue;
    const PropertyValue* inherited = nullptr;
    if (HasFlag(metadata->flags, PropertyMetadataFlags::Inherits)) {
        DependencyObject* parent = InheritanceParent(*entry.object);
        while (parent != nullptr && property->MetadataFor(parent->RuntimeType()) == nullptr)
            parent = InheritanceParent(*parent);
        if (parent != nullptr) {
            Base::Result<PropertyValue> value = parent->GetValue(entry.property);
            if (!value) return value.GetStatus();
            inheritedValue = std::move(value).Value(); inherited = &inheritedValue;
        }
    }
    Base::Result<void> stored = entry.object->ApplyInheritedValueInternal(entry.property, inherited);
    if (!stored) return stored.GetStatus();
    return entry.object->RecomputeEffectiveValueInternal(entry.property);
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
    const std::uint64_t boundary = nextQueueSequence_ - 1U;
    std::uint32_t processed = 0U;

    while (true) {
        std::uint32_t selected = EffectiveInvalidIndex;
        std::uint64_t selectedSequence = UINT64_MAX;
        for (std::uint32_t index = 0U;
             index < entries_.Size();
             ++index) {
            const Entry& entry = entries_[index];
            if (entry.queued &&
                entry.queueSequence <= boundary &&
                entry.queueSequence < selectedSequence) {
                selected = index;
                selectedSequence = entry.queueSequence;
            }
        }
        if (selected == EffectiveInvalidIndex) break;

        Entry& entry = entries_[selected];
        entry.queued = false;
        Base::Result<void> applied = Apply(entry);
        if (!applied) {
            entry.queued = true;
            return applied.GetStatus();
        }
        ++processed;
    }
    return processed;
}

Base::Result<EffectiveValueDiagnostics> EffectiveValueEngine::Diagnostics(
    const DependencyObject& object, DependencyPropertyHandle property) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    return object.GetValueSourceInfo(property);
}

Base::Result<void> EffectiveValueEngine::DetachObject(DependencyObject& object) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    std::uint32_t entry = 0U;
    while (entry < entries_.Size()) {
        if (entries_[entry].object == &object) {
            Base::Result<void> cleared = object.DropEngineValueStateInternal(entries_[entry].property);
            if (!cleared) return cleared.GetStatus();
            RemoveEntry(entry);
        } else ++entry;
    }
    std::uint32_t parent = 0U;
    while (parent < parents_.Size()) {
        if (parents_[parent].child == &object || parents_[parent].parent == &object)
            RemoveParent(parent); else ++parent;
    }
    RemoveInheritanceSubscription(object);
    return {};
}

std::uint32_t EffectiveValueEngine::PendingPropertyCount() const noexcept {
    std::uint32_t count = 0U;
    for (const Entry& entry : entries_) {
        if (entry.queued) ++count;
    }
    return count;
}


void EffectiveValueEngine::RemoveEntry(
    std::uint32_t index) noexcept {
    AERO_ASSERT(index < entries_.Size());
    for (std::uint32_t current = index + 1U;
         current < entries_.Size();
         ++current) {
        entries_[current - 1U] = std::move(entries_[current]);
    }
    entries_.PopBack();
}

void EffectiveValueEngine::RemoveParent(
    std::uint32_t index) noexcept {
    AERO_ASSERT(index < parents_.Size());
    for (std::uint32_t current = index + 1U;
         current < parents_.Size();
         ++current) {
        parents_[current - 1U] = parents_[current];
    }
    parents_.PopBack();
}

void EffectiveValueEngine::PropertyChangesHook(
    void* context) noexcept {
    auto* engine = static_cast<EffectiveValueEngine*>(context);
    static_cast<void>(engine->Flush());
}

} // namespace Aero::Meta

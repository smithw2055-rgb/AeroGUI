#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>

#include <cstdint>
#include <limits>
#include <utility>

namespace Aero::Core {
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
    MetadataBehaviorRegistrationStore& behaviors) noexcept
    : typeRegistry_(&typeRegistry),
      behaviorRegistrations_(&behaviors),
      properties_(),
      memberIndex_() {}

Base::Result<void> DependencyPropertyRegistry::ValidateMetadata(
    TypeId valueType,
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
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property value kind does not match the registered type");
    }

    if (value.Type() != property.ValueType() &&
        !typeRegistry_->IsDerivedFrom(value.Type(), property.ValueType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property value type is not assignable to the property type");
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
DependencyPropertyRegistry::TryRegister(
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
        registration.valueType, registration.metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    if (properties_.Size() == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Dependency property registry capacity limit reached");
    }

    DependencyProperty property;
    Base::Result<void> nameResult = property.name_.TryAssign(registration.name);
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
    Base::Result<void> metadataResult = property.metadata_.TryPushBack(
        std::move(ownerMetadata));
    if (!metadataResult) {
        return metadataResult.GetStatus();
    }

    Base::Result<void> reserveResult = properties_.TryReserve(
        properties_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }
    reserveResult = memberIndex_.TryReserve(memberIndex_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    if (property.IsReadOnly() &&
        nextReadOnlySecret_ == std::numeric_limits<std::uint64_t>::max()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Dependency property read-only key space is exhausted");
    }

    const MemberId member = MakeMemberId(
        registration.ownerType, MemberKind::Property, registration.name);
    property.handle_.value = member;
    if (property.IsReadOnly()) {
        property.readOnlySecret_ = Base::MixHash64(
            property.handle_.value ^ nextReadOnlySecret_);
        if (property.readOnlySecret_ == 0U) {
            property.readOnlySecret_ = 1U;
        }
    }

    const std::uint32_t propertyIndex = properties_.Size();
    Base::Result<void> appendResult = properties_.TryPushBack(
        std::move(property));
    AERO_ASSERT(appendResult);
    if (!appendResult) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved dependency property append unexpectedly failed");
    }

    Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> indexResult =
        memberIndex_.TryInsert(member, propertyIndex);
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
    Base::Result<MemberId> registered = MetadataRegistrationTypes(
        *typeRegistry_, *behaviorRegistrations_).TryRegisterProperty(
            registration.ownerType, metaProperty);
    if (!registered) {
        static_cast<void>(memberIndex_.Erase(member));
        properties_.PopBack();
        return registered.GetStatus();
    }
    AERO_ASSERT(registered.Value() == member);
    if (properties_[propertyIndex].IsReadOnly()) {
        ++nextReadOnlySecret_;
    }

    DependencyPropertyRegistrationResult result;
    result.property.value = member;
    const DependencyProperty& stored = properties_[propertyIndex];
    if (stored.IsReadOnly()) {
        result.readOnlyKey.registry_ = this;
        result.readOnlyKey.property_ = result.property;
        result.readOnlyKey.secret_ = stored.readOnlySecret_;
    }
    return result;
}

Base::Result<void> DependencyPropertyRegistry::TryAddOwner(
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
        property.ValueType(), metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    Base::Result<void> reserveResult = property.metadata_.TryReserve(
        property.metadata_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }
    reserveResult = memberIndex_.TryReserve(memberIndex_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    DependencyProperty::MetadataEntry entry;
    entry.forType = ownerType;
    entry.owner = true;
    entry.metadata = metadata;
    Base::Result<void> appendResult = property.metadata_.TryPushBack(
        std::move(entry));
    AERO_ASSERT(appendResult);
    if (!appendResult) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved owner metadata append unexpectedly failed");
    }

    Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult> indexResult =
        memberIndex_.TryInsert(
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
    Base::Result<MemberId> alias = MetadataRegistrationTypes(
        *typeRegistry_, *behaviorRegistrations_).TryRegisterProperty(
            ownerType, metaProperty);
    if (!alias) {
        static_cast<void>(memberIndex_.Erase(aliasMember));
        property.metadata_.PopBack();
        return alias.GetStatus();
    }
    AERO_ASSERT(alias.Value() == aliasMember);
    return {};
}

Base::Result<void> DependencyPropertyRegistry::TryOverrideMetadata(
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
        property.ValueType(), metadata);
    if (!validation) {
        return validation.GetStatus();
    }

    Base::Result<void> reserveResult = property.metadata_.TryReserve(
        property.metadata_.Size() + 1U);
    if (!reserveResult) {
        return reserveResult.GetStatus();
    }

    DependencyProperty::MetadataEntry entry;
    entry.forType = forType;
    entry.owner = false;
    entry.metadata = metadata;
    return property.metadata_.TryPushBack(std::move(entry));
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
                property.ValueType(), entry.metadata);
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
    return registered != nullptr && registered->IsReadOnly() &&
        registered->readOnlySecret_ == key->secret_;
}

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
    : DispatcherObject(*GetCurrentObjectServices().dispatcher),
      registry_(GetCurrentObjectServices().dependencyProperties),
      runtimeType_(runtimeType),
      objectServicesAvailable_(HasCurrentObjectServices()),
      values_(),
      updateStack_(),
      changeHandlers_() {}

Base::Result<void> DependencyObject::VerifyReady() const noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!objectServicesAvailable_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyObject was created without an ObjectServicesScope");
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

Base::Result<PropertyValue> DependencyObject::GetValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }

    const DependencyProperty* property = registry_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex
        ? values_[index].effectiveValue
        : metadata->defaultValue;
}

Base::Result<PropertyValue> DependencyObject::ReadLocalValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }

    const DependencyProperty* property = registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex || !values_[index].hasLocal) {
        return PropertyValue::Unset();
    }
    return values_[index].localValue;
}

Base::Result<EffectiveValueSource> DependencyObject::GetValueSource(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }

    const DependencyProperty* property = registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex
        ? values_[index].source
        : EffectiveValueSource::Default;
}

Base::Result<void> DependencyObject::SetValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetLocal, &value);
}

Base::Result<void> DependencyObject::SetValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetLocal, &value);
}

Base::Result<void> DependencyObject::SetCurrentValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetCurrent, &value);
}

Base::Result<void> DependencyObject::SetCurrentValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetCurrent, &value);
}

Base::Result<void> DependencyObject::SetReadOnlyCurrentValue(
    DependencyPropertyHandle propertyHandle,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const DependencyProperty* property = registry_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    if (!property->IsReadOnly()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Framework state update requires a read-only property");
    }
    DependencyPropertyKey key;
    key.registry_ = registry_;
    key.property_ = propertyHandle;
    key.secret_ = property->readOnlySecret_;
    return ApplyChange(
        propertyHandle, &key, ChangeKind::SetCurrent, &value);
}

Base::Result<void> DependencyObject::ClearValue(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::Clear, nullptr);
}

Base::Result<void> DependencyObject::ClearValue(
    const DependencyPropertyKey& key) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::Clear, nullptr);
}

Base::Result<void> DependencyObject::CoerceValue(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::ReCoerce, nullptr);
}

Base::Result<void> DependencyObject::TryAddValueChangedHandler(
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
    return changeHandlers_.TryPushBack(std::move(record));
}

Base::Result<bool> DependencyObject::RemoveValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
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

Base::Result<PropertyInvalidationFlags>
DependencyObject::TakeInvalidations() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    const PropertyInvalidationFlags result = invalidations_;
    invalidations_ = PropertyInvalidationFlags::None;
    return result;
}

Base::Result<DependencyObject::MutationScope>
DependencyObject::BeginMutation(
    DependencyPropertyHandle property) noexcept {
    for (MemberId active : updateStack_) {
        if (active == property.value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Recursive mutation of the same dependency property is not allowed");
        }
    }

    Base::Result<void> pushed = updateStack_.TryPushBack(property.value);
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

Base::Result<void> DependencyObject::ApplyChange(
    DependencyPropertyHandle propertyHandle,
    const DependencyPropertyKey* key,
    ChangeKind kind,
    const PropertyValue* requestedValue) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }

    const DependencyProperty* property = registry_->Find(propertyHandle);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }

    if (kind != ChangeKind::ReCoerce && property->IsReadOnly() &&
        !registry_->ValidateKey(propertyHandle, key)) {
        return ReadOnlyStatus();
    }
    if ((kind == ChangeKind::SetLocal || kind == ChangeKind::SetCurrent) &&
        requestedValue == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property set operation requires a value");
    }

    Base::Result<MutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) {
        return mutationResult.GetStatus();
    }
    MutationScope mutation = std::move(mutationResult).Value();

    std::uint32_t entryIndex = FindEntryIndex(propertyHandle);
    const bool hadEntry = entryIndex != InvalidIndex;

    PropertyValue oldEffective = hadEntry
        ? values_[entryIndex].effectiveValue
        : metadata->defaultValue;
    EffectiveValueSource oldSource = hadEntry
        ? values_[entryIndex].source
        : EffectiveValueSource::Default;

    bool hasLocal = hadEntry && values_[entryIndex].hasLocal;
    bool hasCurrent = hadEntry && values_[entryIndex].hasCurrent;
    PropertyValue localValue = hasLocal
        ? values_[entryIndex].localValue
        : PropertyValue::Unset();
    PropertyValue currentValue = hasCurrent
        ? values_[entryIndex].currentValue
        : PropertyValue::Unset();

    switch (kind) {
    case ChangeKind::SetLocal:
        localValue = *requestedValue;
        hasLocal = true;
        currentValue = PropertyValue::Unset();
        hasCurrent = false;
        break;
    case ChangeKind::SetCurrent:
        currentValue = *requestedValue;
        hasCurrent = true;
        break;
    case ChangeKind::Clear:
        localValue = PropertyValue::Unset();
        currentValue = PropertyValue::Unset();
        hasLocal = false;
        hasCurrent = false;
        break;
    case ChangeKind::ReCoerce:
        break;
    }

    const EffectiveValueSource newSource = hasCurrent
        ? EffectiveValueSource::Current
        : (hasLocal ? EffectiveValueSource::Local
                    : EffectiveValueSource::Default);
    const PropertyValue& baseValue = hasCurrent
        ? currentValue
        : (hasLocal ? localValue : metadata->defaultValue);

    Base::Result<PropertyValue> evaluated = registry_->EvaluateValue(
        *this, *property, *metadata, baseValue);
    if (!evaluated) {
        return evaluated.GetStatus();
    }
    PropertyValue newEffective = std::move(evaluated).Value();

    const bool shouldStore = hasLocal || hasCurrent ||
        newEffective != metadata->defaultValue;
    if (shouldStore && !hadEntry) {
        if (values_.Size() == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "DependencyObject sparse value table limit reached");
        }
        Base::Result<void> reserve = values_.TryReserve(values_.Size() + 1U);
        if (!reserve) {
            return reserve.GetStatus();
        }
    }

    // Coercion callbacks may mutate other properties and move the sparse table.
    entryIndex = FindEntryIndex(propertyHandle);
    if (shouldStore) {
        if (entryIndex == InvalidIndex) {
            EffectiveValueEntry entry;
            entry.property = propertyHandle;
            entry.localValue = localValue;
            entry.currentValue = currentValue;
            entry.effectiveValue = newEffective;
            entry.source = newSource;
            entry.hasLocal = hasLocal;
            entry.hasCurrent = hasCurrent;
            Base::Result<void> append = values_.TryPushBack(std::move(entry));
            AERO_ASSERT(append);
            if (!append) {
                return Base::Status::Failure(
                    Base::ErrorCode::InternalError,
                    "Reserved sparse value append unexpectedly failed");
            }
        } else {
            EffectiveValueEntry& entry = values_[entryIndex];
            entry.localValue = localValue;
            entry.currentValue = currentValue;
            entry.effectiveValue = newEffective;
            entry.source = newSource;
            entry.hasLocal = hasLocal;
            entry.hasCurrent = hasCurrent;
        }
    } else if (entryIndex != InvalidIndex) {
        RemoveEntry(entryIndex);
    }

    if (newEffective != oldEffective) {
        const PropertyInvalidationFlags changeInvalidations =
            AccumulateInvalidations(metadata->flags);
        if (!metadata->changed.Empty()) {
            const DependencyPropertyChangedEventArgs args{
                propertyHandle,
                oldEffective,
                newEffective,
                oldSource,
                newSource
            };
            metadata->changed(*this, args);
            NotifyValueChanged(args);
        } else {
            const DependencyPropertyChangedEventArgs args{
                propertyHandle,
                oldEffective,
                newEffective,
                oldSource,
                newSource
            };
            NotifyValueChanged(args);
        }
        return OnPropertyInvalidated(changeInvalidations);
    }
    return {};
}

Base::Result<void> DependencyObject::OnPropertyInvalidated(
    PropertyInvalidationFlags) noexcept {
    return {};
}

void DependencyObject::RemoveEntry(std::uint32_t index) noexcept {
    AERO_ASSERT(index < values_.Size());
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
        if (record.active && record.property == args.property) {
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

} // namespace Aero::Core

// Consolidated implementation. Keep sections ordered by dependency.

// ===== DependencyProperty =====

#include <Aero/DependencyProperty.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/core/PropertyStore.hpp"
#include <Aero/Collections.hpp>
#include "gui/media/AnimationEngine.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Layout.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include "gui/core/ValueConversion.hpp"

#include <cstdio>
#include <cstdint>
#include <limits>
#include <new>
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

    if (metadata == nullptr) {
        metadata = Meta::CurrentObjectFactory().metadata;
    }
    const Meta::TypeRegistry* types =
        (metadata != nullptr && metadata->IsReady())
            ? &metadata->Types()
            : property.Types();

    if (targetType == Meta::TypeOf<Base::String>() &&
        value.Kind() == Meta::ValueKind::Object &&
        value.IsNullObject()) {
        return Meta::ValueCodec<Base::String>::Encode(Base::String{});
    }

    {
        Base::Result<Meta::PropertyValue> defaultConverted =
            Base::ValueConversion::TryConvertDefaultValue(
                value, targetType, types, /*autoLengthAsZero=*/true);
        if (defaultConverted ||
            (targetType == Meta::TypeOf<bool>() &&
             value.Type() == Meta::TypeOf<::Aero::Nullable<bool>>())) {
            return defaultConverted;
        }
    }

    if (value.Kind() == Meta::ValueKind::String) {
        const Base::StringView text = value.AsString();
        bool convertedViaMetadata = false;
        Base::Status metadataError{};
        if (metadata != nullptr && metadata->IsReady()) {
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(targetType, text);
            if (converted) {
                value = std::move(converted).Value();
                convertedViaMetadata = true;
            } else {
                metadataError = converted.GetStatus();
            }
        }
        if (!convertedViaMetadata &&
            (metadataError.IsOk() ||
             metadataError.code == Base::ErrorCode::Unsupported ||
             metadataError.code == Base::ErrorCode::NotFound)) {
            Base::Result<Meta::PropertyValue> fallback =
                Base::ValueConversion::ConvertTextToValueFallback(types, targetType, text);
            if (fallback) {
                value = std::move(fallback).Value();
            } else if (!metadataError.IsOk()) {
                return metadataError;
            } else {
                return fallback.GetStatus();
            }
        } else if (!convertedViaMetadata) {
            return metadataError;
        }
    }

    if (value.Kind() == Meta::ValueKind::Object &&
        value.Type() != targetType) {
        if (value.IsNullObject()) {
            if (types != nullptr) {
                const Meta::TypeInfo* targetInfo =
                    types->FindType(targetType);
                if (targetInfo != nullptr &&
                    (static_cast<std::uint32_t>(targetInfo->Flags()) &
                     static_cast<std::uint32_t>(Meta::TypeFlags::ValueType)) != 0U) {
                    const Meta::PropertyMetadata* propMeta =
                        property.MetadataFor(property.RegisteredOwnerType());
                    if (propMeta != nullptr && !propMeta->defaultValue.IsUnset()) {
                        return propMeta->defaultValue;
                    }
                }
            }
            value = Meta::PropertyValue::NullObject(targetType);
        } else if (value.AsObject()) {
            const Meta::TypeId objectType =
                value.AsObject()->RuntimeType() != Meta::InvalidTypeId
                    ? value.AsObject()->RuntimeType()
                    : value.Type();
            if (targetType == Meta::TypeOf<Base::Object>() ||
                (types != nullptr &&
                 types->IsAssignableFrom(
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
    if (typeRegistry_ == nullptr || forType == InvalidTypeId || metadata_.Empty()) {
        return nullptr;
    }

    // Fast path 1: exact registered-owner match (metadata_[0] is always the registered owner).
    if (forType == registeredOwnerType_) {
        return &metadata_[0].metadata;
    }

    // Fast path 2: direct-mapped 8-slot cache lookup (zero element shifting).
    const std::uint32_t cacheSlot =
        static_cast<std::uint32_t>(forType ^ (forType >> 17U)) & (kMetadataCacheSlots - 1U);
    if (metadataCache_[cacheSlot].forType == forType) {
        return metadataCache_[cacheSlot].metadata;
    }

    // Fast path 3: attached property with no overrides applies its owner metadata to any valid type.
    if (metadata_.Size() == 1U && IsAttached()) {
        if (typeRegistry_->FindType(forType) != nullptr) {
            const PropertyMetadata* attachedResult = &metadata_[0].metadata;
            metadataCache_[cacheSlot].forType = forType;
            metadataCache_[cacheSlot].metadata = attachedResult;
            return attachedResult;
        }
        return nullptr;
    }

    const PropertyMetadata* result = nullptr;
    const TypeInfo* info = typeRegistry_->FindType(forType);
    if (info != nullptr && !info->AncestorChain().Empty()) {
        if (metadata_.Size() == 1U) {
            if (ownerTreeIn_ != 0U && info->TreeIn() != 0U) {
                if (static_cast<std::uint32_t>(info->TreeIn()) - static_cast<std::uint32_t>(ownerTreeIn_) <=
                    static_cast<std::uint32_t>(ownerTreeOut_) - static_cast<std::uint32_t>(ownerTreeIn_)) {
                    result = &metadata_[0].metadata;
                }
            } else if (const TypeInfo* ownerInfo = typeRegistry_->FindType(registeredOwnerType_)) {
                if (info->TreeIn() != 0U && ownerInfo->TreeIn() != 0U) {
                    if (info->IsSubtreeOf(*ownerInfo)) {
                        result = &metadata_[0].metadata;
                    }
                } else {
                    for (TypeId current : info->AncestorChain()) {
                        if (current == registeredOwnerType_) {
                            result = &metadata_[0].metadata;
                            break;
                        }
                    }
                }
            }
        } else {
            for (TypeId current : info->AncestorChain()) {
                const MetadataEntry* exact = FindMetadataExact(current);
                if (exact != nullptr) {
                    result = &exact->metadata;
                    break;
                }
            }
        }
    } else {
        TypeId current = forType;
        std::uint32_t remaining = typeRegistry_->TypeCount() + 1U;
        while (current != InvalidTypeId && remaining > 0U) {
            const MetadataEntry* exact = FindMetadataExact(current);
            if (exact != nullptr) {
                result = &exact->metadata;
                break;
            }

            const TypeInfo* type = current == forType ? info : typeRegistry_->FindType(current);
            if (type == nullptr) {
                break;
            }
            current = type->BaseType();
            --remaining;
        }
    }
    if (result == nullptr && IsAttached()) {
        result = &metadata_[0].metadata;
    }

    if (result != nullptr) {
        metadataCache_[cacheSlot].forType = forType;
        metadataCache_[cacheSlot].metadata = result;
    }

    return result;
}

namespace {
std::atomic_flag g_liveRegistriesLock = ATOMIC_FLAG_INIT;
DependencyPropertyRegistry* g_liveRegistriesHead = nullptr;
std::uint16_t g_nextRegistryEpoch = 1U;

struct LiveRegistryLockGuard {
    LiveRegistryLockGuard() noexcept {
        while (g_liveRegistriesLock.test_and_set(std::memory_order_acquire)) {
        }
    }
    ~LiveRegistryLockGuard() noexcept {
        g_liveRegistriesLock.clear(std::memory_order_release);
    }
};

constexpr std::uint32_t kFastPropertyCacheSlots = 4096U;
struct FastPropertyCacheSlot {
    MemberId requestedId = InvalidMemberId;
    DependencyPropertyHandle resolvedHandle{};
};
FastPropertyCacheSlot g_fastPropertyCache[kFastPropertyCacheSlots]{};

void CacheFastPropertyHandle(
    MemberId requestedId,
    DependencyPropertyHandle resolvedHandle) noexcept {
    if (requestedId == InvalidMemberId || !resolvedHandle.HasGlobalIndex()) {
        return;
    }
    const std::uint32_t slot =
        static_cast<std::uint32_t>(requestedId ^ (requestedId >> 32U)) &
        (kFastPropertyCacheSlots - 1U);
    g_fastPropertyCache[slot].requestedId = requestedId;
    g_fastPropertyCache[slot].resolvedHandle = resolvedHandle;
}
} // namespace

DependencyPropertyHandle ResolveDependencyPropertyHandle(
    DependencyPropertyHandle handle) noexcept {
    if (handle.HasGlobalIndex() || handle.value == InvalidMemberId) {
        return handle;
    }
    const std::uint32_t slot =
        static_cast<std::uint32_t>(handle.value ^ (handle.value >> 32U)) &
        (kFastPropertyCacheSlots - 1U);
    const FastPropertyCacheSlot& cached = g_fastPropertyCache[slot];
    if (cached.requestedId == handle.value) {
        return cached.resolvedHandle;
    }
    return handle;
}

DependencyPropertyRegistry::DependencyPropertyRegistry(
    TypeRegistry& typeRegistry,
    BehaviorTable& behaviors) noexcept
    : typeRegistry_(&typeRegistry),
      behaviorRegistrations_(&behaviors),
      properties_(),
      memberIndex_() {
    LiveRegistryLockGuard lock;
    std::uint16_t assigned = g_nextRegistryEpoch++;
    if (assigned == 0U) {
        assigned = g_nextRegistryEpoch++;
    }
    epoch_ = assigned;
    prevLive_ = nullptr;
    nextLive_ = g_liveRegistriesHead;
    if (g_liveRegistriesHead != nullptr) {
        g_liveRegistriesHead->prevLive_ = this;
    }
    g_liveRegistriesHead = this;
}

DependencyPropertyRegistry::~DependencyPropertyRegistry() noexcept {
    Threading::InvalidateRegistryContextSlots(this);
    LiveRegistryLockGuard lock;
    if (prevLive_ != nullptr) {
        prevLive_->nextLive_ = nextLive_;
    } else if (g_liveRegistriesHead == this) {
        g_liveRegistriesHead = nextLive_;
    }
    if (nextLive_ != nullptr) {
        nextLive_->prevLive_ = prevLive_;
    }
    prevLive_ = nullptr;
    nextLive_ = nullptr;
    epoch_ = 0U;
}

bool DependencyPropertyRegistry::IsLive(
    const DependencyPropertyRegistry* registry,
    std::uint16_t epoch) noexcept {
    if (registry == nullptr) return false;
    LiveRegistryLockGuard lock;
    for (const DependencyPropertyRegistry* curr = g_liveRegistriesHead;
         curr != nullptr;
         curr = curr->nextLive_) {
        if (curr == registry) {
            return epoch == 0U || curr->epoch_ == epoch;
        }
    }
    return false;
}

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
    if (value.IsDynamicResource()) {
        return {};
    }
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

    const TypeId expectedTypeId = property.ValueType();
    const TypeId actualTypeId = value.Type();
    const TypeInfo* expected = typeRegistry_->FindType(expectedTypeId);
    const TypeInfo* actual = (actualTypeId == expectedTypeId)
        ? expected
        : typeRegistry_->FindType(actualTypeId);
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

    const Base::Object* objVal = (objectValue && !value.IsNullObject()) ? value.AsObject().Get() : nullptr;
    const Meta::TypeId objRuntimeType = objVal != nullptr ? objVal->RuntimeType() : InvalidTypeId;
    const Meta::TypeId effectiveValueType =
        (objRuntimeType != InvalidTypeId && typeRegistry_->FindType(objRuntimeType) != nullptr)
            ? objRuntimeType
            : actualTypeId;
    if (effectiveValueType != expectedTypeId &&
        !typeRegistry_->IsDerivedFrom(effectiveValueType, expectedTypeId)) {
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
    PropertyValue dynamicResolved;
    const PropertyValue* resolvedPtr = &baseValue;
    if (baseValue.IsDynamicResource()) {
        const Base::StringView key = baseValue.AsDynamicResourceKey();
        ResourceValue resource;
        if (auto* fe = ::Aero::TryCast<FrameworkElement>(&object)) {
            resource = fe->TryFindResource(key);
        }
        if (resource.IsUnset()) {
            dynamicResolved = metadata.defaultValue;
        } else if (resource.Type() != property.ValueType()) {
            Base::Result<PropertyValue> normalized =
                ::Aero::NormalizeValueForProperty(
                    Meta::CurrentObjectFactory().metadata, property, resource);
            dynamicResolved = normalized ? std::move(normalized).Value() : std::move(resource);
        } else {
            dynamicResolved = std::move(resource);
        }
        resolvedPtr = &dynamicResolved;
    }

    Base::Result<void> validation = ValidateValue(
        property, metadata, *resolvedPtr);
    if (!validation) {
        return validation.GetStatus();
    }
    // OOP fallback: the instance virtuals run for every evaluation, with or
    // without a metadata delegate (AND semantics). Defaults are pass-through
    // (Validate true / Coerce identity), so registered behavior is unchanged
    // and authors can override instead of registering per-DP delegates.
    if (!object.ValidateValueCore(property.Handle(), *resolvedPtr)) {
        return ValidationFailedStatus();
    }

    // Coercion is virtual-only: per-DP coerce delegates were removed.
    // Overrides adjust the value (or return Unset to reject); the default
    // returns the base value unchanged.
    PropertyValue adjusted =
        object.CoerceValueCore(property.Handle(), *resolvedPtr);
    if (adjusted.IsUnset()) {
        return ValidationFailedStatus();
    }
    if (adjusted != *resolvedPtr) {
        validation = ValidateValue(property, metadata, adjusted);
        if (!validation) {
            return validation.GetStatus();
        }
    }
    return adjusted;
}

PropertyFlags DependencyPropertyRegistry::ToTypeRegistryFlags(
    DependencyPropertyFlags propertyFlags,
    FrameworkPropertyMetadataOptions metadataFlags) noexcept {
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
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::Inherits)) {
        result = result | PropertyFlags::Inherits;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsMeasure)) {
        result = result | PropertyFlags::AffectsMeasure;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsArrange)) {
        result = result | PropertyFlags::AffectsArrange;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsRender)) {
        result = result | PropertyFlags::AffectsRender;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsParentMeasure)) {
        result = result | PropertyFlags::AffectsParentMeasure;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsParentArrange)) {
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
    property.computedReader_ = registration.metadata.computedReader;

    DependencyProperty::MetadataEntry ownerMetadata;
    ownerMetadata.forType = registration.ownerType;
    ownerMetadata.owner = true;
    ownerMetadata.metadata = registration.metadata;
    property.metadata_.PushBack(
        std::move(ownerMetadata));

    properties_.Reserve(
        properties_.Size() + 1U);
    memberIndex_.Reserve(memberIndex_.Size() + 1U);

    if (property.GetIsReadOnly() &&
        nextReadOnlySecret_ == std::numeric_limits<std::uint64_t>::max()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Dependency property read-only key space is exhausted");
    }

    const MemberId member = MakeMemberId(
        registration.ownerType, MemberKind::Property, registration.name);
    const std::uint32_t propertyIndex = properties_.Size();
    const std::uint16_t assignedGlobalIndex = propertyIndex < 0xFFFFU
        ? static_cast<std::uint16_t>(propertyIndex)
        : 0xFFFFU;
    property.globalIndex_ = assignedGlobalIndex;
    property.handle_.value = member;
    property.handle_.globalIndex = assignedGlobalIndex;
    if (property.GetIsReadOnly()) {
        property.readOnlySecret_ = Base::MixHash64(
            property.handle_.value ^ nextReadOnlySecret_);
        if (property.readOnlySecret_ == 0U) {
            property.readOnlySecret_ = 1U;
        }
    }

    properties_.PushBack(
        std::move(property));

    Base::HashMap<MemberId, std::uint32_t>::InsertResult indexResult =
        memberIndex_.Insert(member, propertyIndex);
    AERO_ASSERT(indexResult.inserted);
    if (!indexResult.inserted) {
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
    const DependencyProperty& stored = properties_[propertyIndex];
    result.property = stored.Handle();
    CacheFastPropertyHandle(member, result.property);
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
    DependencyPropertyFlags flags,
    Base::StringView aliasName) noexcept {
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
    const Base::StringView actualName =
        !aliasName.Empty() ? aliasName : property.Name();
    if (property.FindMetadataExact(ownerType) != nullptr ||
        typeRegistry_->FindProperty(ownerType, actualName, false) != nullptr) {
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

    property.metadata_.Reserve(
        property.metadata_.Size() + 1U);
    memberIndex_.Reserve(memberIndex_.Size() + 1U);

    DependencyProperty::MetadataEntry entry;
    entry.forType = ownerType;
    entry.owner = true;
    entry.metadata = metadata;
    entry.flags = flags;
    static_cast<void>(entry.aliasName.Assign(actualName));
    property.metadata_.PushBack(
        std::move(entry));
    property.ClearMetadataCache();

    Base::HashMap<MemberId, std::uint32_t>::InsertResult indexResult =
        memberIndex_.Insert(
            MakeMemberId(ownerType, MemberKind::Property, actualName),
            propertyIndex);
    AERO_ASSERT(indexResult.inserted);
    if (!indexResult.inserted) {
        property.metadata_.PopBack();
        property.ClearMetadataCache();
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Reserved owner alias insertion unexpectedly failed");
    }

    const MemberId aliasMember =
        MakeMemberId(ownerType, MemberKind::Property, actualName);
    PropertyRegistration metaProperty;
    metaProperty.name = actualName;
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
        property.ClearMetadataCache();
        return alias.GetStatus();
    }
    AERO_ASSERT(alias.Value() == aliasMember);
    CacheFastPropertyHandle(aliasMember, property.Handle());
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
    property.metadata_.Reserve(
        property.metadata_.Size() + 1U);
    property.ClearMetadataCache();

    const PropertyMetadata* baseMetadata =
        type->BaseType() != InvalidTypeId ? property.MetadataFor(type->BaseType()) : nullptr;
    if (baseMetadata == nullptr) {
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

    DependencyProperty::MetadataEntry entry;
    entry.forType = forType;
    entry.owner = false;
    entry.metadata = metadata;
    if (entry.metadata.changed.Empty() && !baseMetadata->changed.Empty()) {
        entry.metadata.changed = baseMetadata->changed;
    }
    if (entry.metadata.computedReader == nullptr && baseMetadata->computedReader != nullptr) {
        entry.metadata.computedReader = baseMetadata->computedReader;
    }
    if (property.computedReader_ == nullptr && entry.metadata.computedReader != nullptr) {
        property.computedReader_ = entry.metadata.computedReader;
    }
    property.metadata_.PushBack(std::move(entry));
    property.ClearMetadataCache();
    return {};
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

    inheritableProperties_.Clear();
    std::uint32_t inheritableIndex = 0U;
    for (DependencyProperty& property : properties_) {
        bool inherits = false;
        bool hasDefaultOverride = false;
        const PropertyValue* ownerDefault = !property.metadata_.Empty()
            ? &property.metadata_[0].metadata.defaultValue
            : nullptr;
        for (std::uint32_t i = 0U; i < property.metadata_.Size(); ++i) {
            const DependencyProperty::MetadataEntry& entry = property.metadata_[i];
            if (HasFlag(entry.metadata.flags, FrameworkPropertyMetadataOptions::Inherits)) {
                inherits = true;
            }
            if (i > 0U && ownerDefault != nullptr && entry.metadata.defaultValue != *ownerDefault) {
                hasDefaultOverride = true;
            }
        }
        property.hasDerivedDefaultOverride_ = hasDefaultOverride;
        if (inherits) {
            property.inheritanceIndex_ = inheritableIndex < 30U
                ? static_cast<std::uint8_t>(inheritableIndex)
                : 30U;
            ++inheritableIndex;
            inheritableProperties_.PushBack(property.Handle());
        } else {
            property.inheritanceIndex_ = 0xFFU;
        }
    }

    for (DependencyProperty& property : properties_) {
        if (typeRegistry_ != nullptr) {
            if (const TypeInfo* ownerInfo = typeRegistry_->FindType(property.registeredOwnerType_)) {
                property.ownerTreeIn_ = ownerInfo->TreeIn();
                property.ownerTreeOut_ = ownerInfo->TreeOut();
            }
        }
        property.ClearMetadataCache();
    }

    for (const auto& kv : memberIndex_) {
        if (kv.Value() < properties_.Size()) {
            CacheFastPropertyHandle(kv.Key(), properties_[kv.Value()].Handle());
        }
    }

    RebuildFlatIndex();
    frozen_ = true;
    return {};
}

void DependencyPropertyRegistry::RebuildFlatIndex() noexcept {
    flatIndex_.Clear();
    const std::uint32_t entryCount = memberIndex_.Size();
    if (entryCount == 0U) {
        return;
    }
    std::uint32_t capacity = 64U;
    while (capacity < entryCount * 4U) {
        capacity <<= 1U;
    }
    flatIndex_.Resize(capacity);
    if (flatIndex_.Size() < capacity) {
        flatIndex_.Clear();
        return;
    }
    const std::uint32_t mask = capacity - 1U;
    for (const auto& kv : memberIndex_) {
        const MemberId key = kv.Key();
        const std::uint32_t val = kv.Value();
        std::uint32_t slot =
            static_cast<std::uint32_t>(key ^ (key >> 32U)) & mask;
        while (flatIndex_[slot].member != InvalidMemberId) {
            slot = (slot + 1U) & mask;
        }
        flatIndex_[slot].member = key;
        flatIndex_[slot].index = val;
    }
}

std::uint32_t DependencyPropertyRegistry::FindIndex(MemberId member) const noexcept {
    if (member == InvalidMemberId) {
        return InvalidIndex;
    }
    if (!flatIndex_.Empty()) {
        const std::uint32_t mask = flatIndex_.Size() - 1U;
        std::uint32_t slot =
            static_cast<std::uint32_t>(member ^ (member >> 32U)) & mask;
        for (;;) {
            const FlatIndexSlot& entry = flatIndex_[slot];
            if (entry.member == member) {
                return entry.index;
            }
            if (entry.member == InvalidMemberId) {
                return InvalidIndex;
            }
            slot = (slot + 1U) & mask;
        }
    }
    const std::uint32_t* index = memberIndex_.Find(member);
    return index != nullptr ? *index : InvalidIndex;
}

const DependencyProperty* DependencyPropertyRegistry::Find(
    DependencyPropertyHandle property) const noexcept {
    if (property.globalIndex < properties_.Size()) {
        const DependencyProperty& direct = properties_[property.globalIndex];
        if (direct.handle_.value == property.value) {
            return &direct;
        }
    }
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

MemberId DependencyPropertyRegistry::CanonicalHandle(
    DependencyPropertyHandle property) const noexcept {
    if (property.globalIndex < properties_.Size()) {
        const DependencyProperty& direct = properties_[property.globalIndex];
        if (direct.handle_.value == property.value) {
            return direct.handle_.value;
        }
    }
    const std::uint32_t index = FindIndex(property.value);
    return index != InvalidIndex
        ? properties_[index].Handle().value
        : property.value;
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

DependencyMutationScope::DependencyMutationScope(
    DependencyObject* owner,
    DispatcherReentrancyGuard&& guard) noexcept
    : owner_(owner),
      dispatcherGuard_(std::move(guard)) {}

DependencyMutationScope::DependencyMutationScope(
    DependencyMutationScope&& other) noexcept
    : owner_(other.owner_),
      dispatcherGuard_(std::move(other.dispatcherGuard_)) {
    other.owner_ = nullptr;
}

DependencyMutationScope& DependencyMutationScope::operator=(
    DependencyMutationScope&& other) noexcept {
    if (this != &other) {
        Release();
        owner_ = other.owner_;
        dispatcherGuard_ = std::move(other.dispatcherGuard_);
        other.owner_ = nullptr;
    }
    return *this;
}

DependencyMutationScope::~DependencyMutationScope() {
    Release();
}

void DependencyMutationScope::Release() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    owner_->LeaveMutation();
    owner_ = nullptr;
    dispatcherGuard_.Release();
}

DependencyObject::DependencyObject(TypeId runtimeType) noexcept
    : DispatcherObject(*CurrentObjectFactory().dispatcher),
      valueStore_(nullptr),
      rare_(nullptr),
      compactTypeIndex_(Meta::InternCompactTypeId(runtimeType)),
      objectServicesAvailable_(HasObjectFactory() ? 1U : 0U),
      typeVerified_(0U),
      hasExplicitInheritanceChildren_(0U),
      hasExplicitInheritanceParent_(0U),
      logicalParentMode_(0U) {
    const Meta::ObjectFactoryState factory = CurrentObjectFactory();
    SetContextSlot(Threading::AcquireObjectContextSlot(
        factory.dispatcher,
        factory.dependencyProperties,
        factory.dependencyProperties != nullptr
            ? factory.dependencyProperties->Epoch()
            : 0U));
    AddCoreTypeFlag(Base::kCoreTypeDependencyObject);
}

DependencyObject::~DependencyObject() {
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const std::uint16_t registryEpoch = Threading::ResolveObjectContextRegistryEpoch(ContextSlot());
    if (registry != nullptr &&
        (styleAppIndex_ != UINT16_MAX || unmaterializedQueueMask_ != 0U ||
         hasExplicitInheritanceParent_ || hasExplicitInheritanceChildren_ ||
         valueStore_ != nullptr) &&
        DependencyPropertyRegistry::IsLive(registry, registryEpoch)) {
        registry->NotifyObjectDestroyed(*this);
    }
    if (rare_ != nullptr) {
        delete rare_;
        rare_ = nullptr;
    }
    PropertyStore* store = static_cast<PropertyStore*>(valueStore_);
    if (store != nullptr) {
        for (auto& record : store->entries) {
            (*this).CommitConsumerChange(
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

constexpr std::uint32_t kMaxDirectInheritanceBits = 14U;
constexpr std::uint16_t kOverflowUnmaterializedBit = static_cast<std::uint16_t>(1U << 14U);
constexpr std::uint16_t kUnmaterializedBitsMask = 0x7FFFU;
constexpr std::uint16_t kHasQueuedStoredLinkBit = static_cast<std::uint16_t>(1U << 15U);

constexpr std::uint16_t UnmaterializedBitFor(std::uint8_t inheritanceIndex) noexcept {
    return inheritanceIndex < kMaxDirectInheritanceBits
        ? static_cast<std::uint16_t>(1U << static_cast<std::uint32_t>(inheritanceIndex))
        : kOverflowUnmaterializedBit;
}

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
      registry_(&registry) {}

void EffectiveValueEngine::Shutdown() noexcept {
    if (registry_ != nullptr) {
        registry_->SetInheritanceChangeSink(nullptr);
    }
    ClearQueueLinks();
    DependencyObject* parent = firstExplicitParent_;
    firstExplicitParent_ = nullptr;
    while (parent != nullptr) {
        DependencyObject* nextParent = parent->rare_ != nullptr
            ? parent->rare_->nextExplicitParent
            : nullptr;
        if (parent->rare_ != nullptr) {
            DependencyObject* child = parent->rare_->firstInheritanceChild;
            parent->rare_->firstInheritanceChild = nullptr;
            parent->rare_->nextExplicitParent = nullptr;
            while (child != nullptr) {
                DependencyObject* nextChild = child->rare_ != nullptr
                    ? child->rare_->nextInheritanceSibling
                    : nullptr;
                if (child->rare_ != nullptr) {
                    child->rare_->nextInheritanceSibling = nullptr;
                }
                child->SetExplicitInheritanceParent(nullptr);
                child = nextChild;
            }
            parent->CleanupRareIfEmpty();
        }
        parent->hasExplicitInheritanceChildren_ = false;
        parent = nextParent;
    }
}

EffectiveValueEngine::~EffectiveValueEngine() noexcept {
    Shutdown();
}

Base::Result<void> EffectiveValueEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (initialized_) return {};
    if (!registry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyPropertyRegistry must be frozen before the value engine");
    }

    registry_->SetInheritanceChangeSink(this);
    initialized_ = true;
    return {};
}

Base::Result<void> EffectiveValueEngine::VerifyMutable() const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!initialized_) {
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


Base::Result<void> EffectiveValueEngine::EnqueueStoredEntry(
    DependencyObject& object,
    DependencyPropertyHandle canonicalProperty,
    ::Aero::StoredValueEntry& stored,
    bool descendantsQueued) noexcept {
    if (stored.Queued()) {
        if (descendantsQueued) {
            stored.SetDescendantsQueued(true);
        }
        return {};
    }
    Base::Result<QueueLink*> acquired = AcquireLink();
    if (!acquired) return acquired.GetStatus();
    QueueLink* link = acquired.Value();
    link->object = &object;
    link->property = canonicalProperty;
    link->next = nullptr;
    link->inheritanceIndex = 0xFFU;
    link->unmaterialized = false;
    link->descendantsQueued = descendantsQueued;
    stored.SetQueued(true);
    stored.SetDescendantsQueued(descendantsQueued);
    object.unmaterializedQueueMask_ |= kHasQueuedStoredLinkBit;
    if (queueTail_ != nullptr) {
        queueTail_->next = link;
    } else {
        queueHead_ = link;
    }
    queueTail_ = link;
    ++queueCount_;
    return {};
}

Base::Result<void> EffectiveValueEngine::EnqueueUnmaterialized(
    DependencyObject& object,
    DependencyPropertyHandle canonicalProperty,
    bool descendantsQueued,
    std::uint8_t inheritanceIndex) noexcept {
    if (inheritanceIndex == 0xFFU && registry_ != nullptr) {
        if (const DependencyProperty* prop = registry_->Find(canonicalProperty)) {
            inheritanceIndex = prop->InheritanceIndex();
        }
    }
    const std::uint16_t bit = UnmaterializedBitFor(inheritanceIndex);
    if ((object.unmaterializedQueueMask_ & bit) != 0U) {
        if (inheritanceIndex < kMaxDirectInheritanceBits) {
            return {};
        }
        for (QueueLink* cursor = queueHead_; cursor != nullptr; cursor = cursor->next) {
            if (cursor->object == &object && cursor->property == canonicalProperty) {
                cursor->descendantsQueued = cursor->descendantsQueued || descendantsQueued;
                return {};
            }
        }
    }
    Base::Result<QueueLink*> acquired = AcquireLink();
    if (!acquired) return acquired.GetStatus();
    QueueLink* link = acquired.Value();
    link->object = &object;
    link->property = canonicalProperty;
    link->next = nullptr;
    link->inheritanceIndex = inheritanceIndex;
    link->unmaterialized = true;
    link->descendantsQueued = descendantsQueued;
    object.unmaterializedQueueMask_ |= bit;
    if (queueTail_ != nullptr) {
        queueTail_->next = link;
    } else {
        queueHead_ = link;
    }
    queueTail_ = link;
    ++queueCount_;
    return {};
}

Base::Result<void> EffectiveValueEngine::ResolveAndVerifyObjectProperty(
    const DependencyObject& object,
    DependencyPropertyHandle& inOutProperty,
    const DependencyProperty** outRegistered,
    const PropertyMetadata** outMetadata) const noexcept {
    if (&object.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DependencyObject belongs to another Dispatcher");
    }
    const DependencyProperty* registered = registry_->Find(inOutProperty);
    const PropertyMetadata* metadata = registered != nullptr
        ? registered->MetadataFor(object.RuntimeType())
        : nullptr;
    if (registered == nullptr || metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    inOutProperty = registered->Handle();
    if (outRegistered != nullptr) *outRegistered = registered;
    if (outMetadata != nullptr) *outMetadata = metadata;
    return {};
}

Base::Result<::Aero::StoredValueEntry*>
EffectiveValueEngine::EnsureAndQueueProperty(
    DependencyObject& object,
    DependencyPropertyHandle& inOutProperty,
    const DependencyProperty** outRegistered,
    const PropertyMetadata** outMetadata) noexcept {
    const PropertyMetadata* metadata = nullptr;
    Base::Result<void> verified = ResolveAndVerifyObjectProperty(
        object, inOutProperty, outRegistered, &metadata);
    if (!verified) return verified.GetStatus();
    if (outMetadata != nullptr) *outMetadata = metadata;

    PropertyStore* store = object.Store();
    StoredValueEntry* stored = store != nullptr
        ? store->entries.Find(inOutProperty.value)
        : nullptr;
    if (stored == nullptr) {
        Base::Result<StoredValueEntry*> ensured =
            object.EnsureStoredEntryDirect(inOutProperty, *metadata);
        if (!ensured) return ensured.GetStatus();
        stored = ensured.Value();
    }
    Base::Result<void> queued =
        EnqueueStoredEntry(object, inOutProperty, *stored);
    if (!queued) return queued.GetStatus();
    return stored;
}

Base::Result<void> EffectiveValueEngine::QueueObjectProperty(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<StoredValueEntry*> ensured =
        EnsureAndQueueProperty(object, property);
    if (!ensured) return ensured.GetStatus();
    return {};
}

Base::Result<EffectiveValueEngine::QueueLink*>
EffectiveValueEngine::AcquireLink() noexcept {
    if (linkFree_ != nullptr) {
        QueueLink* link = linkFree_;
        linkFree_ = link->next;
        link->next = nullptr;
        link->inheritanceIndex = 0xFFU;
        link->unmaterialized = false;
        link->descendantsQueued = false;
        return link;
    }
    QueueLink* link = new (std::nothrow) QueueLink();
    if (link == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Effective value queue link allocation failed");
    }
    return link;
}

void EffectiveValueEngine::ClearUnmaterializedQueueBit(
    DependencyObject& object,
    std::uint8_t inheritanceIndex,
    const QueueLink* excludeLink) const noexcept {
    if (inheritanceIndex < kMaxDirectInheritanceBits) {
        object.unmaterializedQueueMask_ &=
            static_cast<std::uint16_t>(~UnmaterializedBitFor(inheritanceIndex));
        return;
    }
    for (const QueueLink* cursor = queueHead_; cursor != nullptr; cursor = cursor->next) {
        if (cursor != excludeLink &&
            cursor->object == &object &&
            cursor->unmaterialized &&
            cursor->inheritanceIndex >= kMaxDirectInheritanceBits) {
            return;
        }
    }
    object.unmaterializedQueueMask_ &=
        static_cast<std::uint16_t>(~kOverflowUnmaterializedBit);
}

void EffectiveValueEngine::RecycleLink(QueueLink* link) noexcept {
    if (link == nullptr) return;
    if (link->unmaterialized && link->object != nullptr) {
        ClearUnmaterializedQueueBit(*link->object, link->inheritanceIndex, link);
    }
    link->object = nullptr;
    link->property = DependencyPropertyHandle{};
    link->inheritanceIndex = 0xFFU;
    link->unmaterialized = false;
    link->descendantsQueued = false;
    link->next = linkFree_;
    linkFree_ = link;
}

void EffectiveValueEngine::ClearQueueLinks() noexcept {
    QueueLink* cursor = queueHead_;
    while (cursor != nullptr) {
        if (cursor->object != nullptr) {
            cursor->object->unmaterializedQueueMask_ = 0U;
        }
        QueueLink* next = cursor->next;
        delete cursor;
        cursor = next;
    }
    queueHead_ = nullptr;
    queueTail_ = nullptr;
    queueCount_ = 0U;
    cursor = linkFree_;
    while (cursor != nullptr) {
        QueueLink* next = cursor->next;
        delete cursor;
        cursor = next;
    }
    linkFree_ = nullptr;
}

void EffectiveValueEngine::UnregisterExplicitInheritanceChild(
    DependencyObject* parent,
    DependencyObject& child) noexcept {
    if (parent == nullptr || !parent->hasExplicitInheritanceChildren_ ||
        parent->rare_ == nullptr) {
        return;
    }
    DependencyObject* prev = nullptr;
    DependencyObject* cur = parent->rare_->firstInheritanceChild;
    while (cur != nullptr) {
        if (cur == &child) {
            if (prev != nullptr) {
                prev->rare_->nextInheritanceSibling =
                    child.rare_ != nullptr ? child.rare_->nextInheritanceSibling : nullptr;
            } else {
                parent->rare_->firstInheritanceChild =
                    child.rare_ != nullptr ? child.rare_->nextInheritanceSibling : nullptr;
            }
            if (child.rare_ != nullptr) {
                child.rare_->nextInheritanceSibling = nullptr;
                child.CleanupRareIfEmpty();
            }
            break;
        }
        prev = cur;
        cur = cur->rare_ != nullptr ? cur->rare_->nextInheritanceSibling : nullptr;
    }
    if (parent->rare_->firstInheritanceChild == nullptr) {
        DependencyObject* prevParent = nullptr;
        DependencyObject* curParent = firstExplicitParent_;
        while (curParent != nullptr) {
            if (curParent == parent) {
                if (prevParent != nullptr) {
                    prevParent->rare_->nextExplicitParent =
                        parent->rare_->nextExplicitParent;
                } else {
                    firstExplicitParent_ = parent->rare_->nextExplicitParent;
                }
                parent->rare_->nextExplicitParent = nullptr;
                break;
            }
            prevParent = curParent;
            curParent = curParent->rare_ != nullptr
                ? curParent->rare_->nextExplicitParent
                : nullptr;
        }
        parent->hasExplicitInheritanceChildren_ = false;
        parent->CleanupRareIfEmpty();
    }
}

DependencyObject* EffectiveValueEngine::InheritanceParent(
    const DependencyObject& child) const noexcept {
    if (DependencyObject* explicitParent = child.ExplicitInheritanceParent()) {
        return explicitParent;
    }
    if (const ::Aero::Media::Visual* visual =
            ::Aero::TryCast<::Aero::Media::Visual>(&child)) {
        if (::Aero::Media::Visual* vParent =
                ::Aero::Media::VisualTreeHelper::GetParent(*visual)) {
            return vParent;
        }
    }
    return ::Aero::LogicalTreeHelper::GetParent(child);
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
    DependencyObject* explicitPrevious = child.ExplicitInheritanceParent();
    const ::Aero::Media::Visual* childVisual =
        ::Aero::TryCast<::Aero::Media::Visual>(&child);
    const bool coveredByVisualTree =
        parent != nullptr &&
        childVisual != nullptr &&
        ::Aero::Media::VisualTreeHelper::GetParent(*childVisual) == parent;
    DependencyObject* explicitTarget = coveredByVisualTree ? nullptr : parent;

    if (explicitPrevious != explicitTarget) {
        UnregisterExplicitInheritanceChild(explicitPrevious, child);
        child.SetExplicitInheritanceParent(explicitTarget);
        if (explicitTarget != nullptr) {
            if (child.ExplicitInheritanceParent() != explicitTarget ||
                explicitTarget->EnsureRare() == nullptr ||
                child.EnsureRare() == nullptr) {
                child.SetExplicitInheritanceParent(nullptr);
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Failed to allocate explicit inheritance parent state");
            }
            if (!explicitTarget->hasExplicitInheritanceChildren_) {
                explicitTarget->rare_->nextExplicitParent = firstExplicitParent_;
                firstExplicitParent_ = explicitTarget;
                explicitTarget->hasExplicitInheritanceChildren_ = true;
            }
            child.rare_->nextInheritanceSibling =
                explicitTarget->rare_->firstInheritanceChild;
            explicitTarget->rare_->firstInheritanceChild = &child;
        }
    }

    if (parent == nullptr && previousParent == nullptr) {
        return {};
    }

    PropertyStore* childStore = child.Store();
    if (childStore != nullptr) {
        for (PropertyStoreEntry& record : childStore->entries) {
            Base::Result<void> queued = EnqueueStoredEntry(
                child, DependencyPropertyHandle{record.Key()}, record.Value());
            if (!queued) return queued.GetStatus();
        }
    }

    struct AncestorNode {
        const PropertyStore* store = nullptr;
        TypeId runtimeType = InvalidTypeId;
    };
    Base::BasicVector<AncestorNode, 16U> parentChain;
    std::uint64_t parentChainBloom = 0ULL;
    for (DependencyObject* anc = parent;
         anc != nullptr && parentChain.Size() < 100U;
         anc = InheritanceParent(*anc)) {
        const PropertyStore* ancStore = anc->Store();
        if (ancStore != nullptr) {
            parentChainBloom |= ancStore->entries.bloomMask_;
        }
        parentChain.PushBack(AncestorNode{ancStore, anc->RuntimeType()});
    }
    Base::BasicVector<AncestorNode, 16U> previousParentChain;
    std::uint64_t prevChainBloom = 0ULL;
    if (previousParent != parent) {
        for (DependencyObject* anc = previousParent;
             anc != nullptr && previousParentChain.Size() < 100U;
             anc = InheritanceParent(*anc)) {
            const PropertyStore* ancStore = anc->Store();
            if (ancStore != nullptr) {
                prevChainBloom |= ancStore->entries.bloomMask_;
            }
            previousParentChain.PushBack(AncestorNode{ancStore, anc->RuntimeType()});
        }
    }

    const bool parentUnchanged =
        (previousParent == parent && explicitPrevious == explicitTarget);
    Base::BasicVector<const DependencyProperty*, 16U> propsToPropagate;

    for (DependencyPropertyHandle inheritableHandle : registry_->InheritableProperties()) {
        const DependencyProperty* prop = registry_->Find(inheritableHandle);
        if (prop == nullptr) continue;

        const std::uint8_t inhIndex = prop->InheritanceIndex();
        StoredValueEntry* childEntry = childStore != nullptr
            ? childStore->entries.Find(inheritableHandle.value)
            : nullptr;

        if (parentUnchanged) {
            const bool alreadyQueuedWithDescendants = childEntry != nullptr
                ? (childEntry->Queued() && childEntry->DescendantsQueued())
                : (inhIndex < kMaxDirectInheritanceBits &&
                   (child.unmaterializedQueueMask_ & UnmaterializedBitFor(inhIndex)) != 0U);
            if (alreadyQueuedWithDescendants) {
                continue;
            }
        }

        const std::uint64_t keyBloom =
            PropertyStore::Entries::KeyBloomBits(inheritableHandle.value);
        const bool hasDerivedDefaultOverride = prop->HasDerivedDefaultOverride();
        const bool childHasInherited =
            childEntry != nullptr && childEntry->HasInherited();

        if (!childHasInherited && !hasDerivedDefaultOverride &&
            (parentChainBloom & keyBloom) != keyBloom &&
            (previousParent == parent || (prevChainBloom & keyBloom) != keyBloom)) {
            continue;
        }

        const PropertyMetadata* childMeta = prop->MetadataFor(child.RuntimeType());
        const PropertyMetadata* ownerMeta = hasDerivedDefaultOverride
            ? prop->MetadataFor(prop->RegisteredOwnerType())
            : childMeta;
        const PropertyValue& baselineDefault = childMeta != nullptr
            ? childMeta->defaultValue
            : (ownerMeta != nullptr ? ownerMeta->defaultValue : PropertyValue{});

        auto hasChainEntry = [&](const Base::BasicVector<AncestorNode, 16U>& chain,
                                 std::uint64_t chainBloom) noexcept {
            const bool mayHaveStored = (chainBloom & keyBloom) == keyBloom;
            if (!mayHaveStored && !hasDerivedDefaultOverride) {
                return false;
            }
            for (const AncestorNode& anc : chain) {
                if (mayHaveStored && anc.store != nullptr &&
                    anc.store->entries.Find(inheritableHandle.value) != nullptr) {
                    return true;
                }
                if (hasDerivedDefaultOverride) {
                    if (const PropertyMetadata* ancMeta = prop->MetadataFor(anc.runtimeType)) {
                        if (ancMeta->defaultValue != baselineDefault ||
                            (ownerMeta != nullptr && ancMeta->defaultValue != ownerMeta->defaultValue)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        if (!childHasInherited &&
            !hasChainEntry(parentChain, parentChainBloom) &&
            (previousParent == parent || !hasChainEntry(previousParentChain, prevChainBloom))) {
            continue;
        }

        const bool childInherits = childMeta != nullptr &&
            HasFlag(childMeta->flags, FrameworkPropertyMetadataOptions::Inherits);
        const bool childShielded =
            childEntry != nullptr && childInherits &&
            childEntry->HasInheritanceShieldingValue() &&
            inheritableHandle != UIElement::IsEnabledProperty.Handle();
        if (childEntry != nullptr) {
            Base::Result<void> queued =
                EnqueueStoredEntry(child, inheritableHandle, *childEntry, /*descendantsQueued=*/!childShielded);
            if (!queued) return queued.GetStatus();
        } else if (childMeta != nullptr) {
            Base::Result<void> queued =
                EnqueueUnmaterialized(child, inheritableHandle, /*descendantsQueued=*/true, inhIndex);
            if (!queued) return queued.GetStatus();
        }
        if (!childShielded) {
            propsToPropagate.PushBack(prop);
        }
    }

    if (!propsToPropagate.Empty()) {
        return PropagateToDescendants(child, propsToPropagate.AsSpan());
    }
    return {};
}

Base::Result<void> EffectiveValueEngine::PropagateToDescendants(
    DependencyObject& root,
    Base::Span<const DependencyProperty* const> properties,
    DependencyPropertyHandle fallbackProperty) noexcept {
    struct CleanupGuard {
        Base::Vector<DependencyObject*>& f;
        Base::HashSet<DependencyObject*>& v;
        ~CleanupGuard() {
            f.Clear();
            v.Clear();
        }
    } guard{frontier_, visited_};

    frontier_.Clear();
    visited_.Clear();
    frontier_.PushBack(&root);
    visited_.Insert(&root);

    auto enqueue = [&](DependencyObject* current, DependencyObject* desc) -> Base::Result<void> {
        if (desc == nullptr || desc == current || !visited_.Insert(desc).inserted) {
            return {};
        }
        const TypeId descType = desc->RuntimeType();
        PropertyStore* descStore = desc->Store();
        bool anyUnshielded = false;

        if (!properties.Empty()) {
            for (const DependencyProperty* prop : properties) {
                const DependencyPropertyHandle propHandle = prop->Handle();
                const std::uint8_t inhIndex = prop->InheritanceIndex();
                const PropertyMetadata* metadata = prop->MetadataFor(descType);
                const bool inherits = metadata != nullptr &&
                    HasFlag(metadata->flags, FrameworkPropertyMetadataOptions::Inherits);
                StoredValueEntry* existing = descStore != nullptr
                    ? descStore->entries.Find(propHandle.value)
                    : nullptr;
                const bool shielded =
                    existing != nullptr && inherits &&
                    existing->HasInheritanceShieldingValue() &&
                    propHandle != UIElement::IsEnabledProperty.Handle();
                if (!shielded) {
                    anyUnshielded = true;
                }
                if (inherits || existing != nullptr) {
                    if (&desc->GetDispatcher() != dispatcher_) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "DependencyObject belongs to another Dispatcher");
                    }
                    if (metadata == nullptr) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Dependency property does not apply to this object type");
                    }
                    Base::Result<void> queued = existing != nullptr
                        ? EnqueueStoredEntry(*desc, propHandle, *existing, /*descendantsQueued=*/!shielded)
                        : EnqueueUnmaterialized(*desc, propHandle, /*descendantsQueued=*/true, inhIndex);
                    if (!queued) return queued.GetStatus();
                }
            }
        } else {
            StoredValueEntry* existing = descStore != nullptr
                ? descStore->entries.Find(fallbackProperty.value)
                : nullptr;
            if (existing != nullptr) {
                if (&desc->GetDispatcher() != dispatcher_) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "DependencyObject belongs to another Dispatcher");
                }
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Dependency property does not apply to this object type");
            }
            anyUnshielded = true;
        }

        if (anyUnshielded) {
            frontier_.PushBack(desc);
        }
        return {};
    };

    auto isDeferredToVisualDescendant = [](DependencyObject* parentObj, DependencyObject* cand) noexcept {
        if (auto* childVis = ::Aero::TryCast<::Aero::Media::Visual>(cand)) {
            ::Aero::Media::Visual* vParent = ::Aero::Media::VisualTreeHelper::GetParent(*childVis);
            if (vParent != nullptr && vParent != parentObj) {
                for (::Aero::Media::Visual* anc = vParent; anc != nullptr; anc = ::Aero::Media::VisualTreeHelper::GetParent(*anc)) {
                    if (anc == parentObj) return true;
                }
            }
        }
        return false;
    };

    for (std::uint32_t cursor = 0U; cursor < frontier_.Size(); ++cursor) {
        DependencyObject* current = frontier_[cursor];
        if (current->hasExplicitInheritanceChildren_ && current->rare_ != nullptr) {
            for (DependencyObject* child = current->rare_->firstInheritanceChild;
                 child != nullptr;
                 child = child->rare_ != nullptr ? child->rare_->nextInheritanceSibling : nullptr) {
                if (isDeferredToVisualDescendant(current, child)) continue;
                Base::Result<void> res = enqueue(current, child);
                if (!res) return res.GetStatus();
            }
        }
        if (auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(current)) {
            const std::uint32_t vCount = ::Aero::Media::VisualTreeHelper::GetChildrenCount(*visual);
            for (std::uint32_t i = 0U; i < vCount; ++i) {
                Base::Result<void> res = enqueue(current, ::Aero::Media::VisualTreeHelper::GetChild(*visual, i));
                if (!res) return res.GetStatus();
            }
        }
        const std::uint32_t lCount = ::Aero::LogicalTreeHelper::GetChildrenCount(*current);
        for (std::uint32_t i = 0U; i < lCount; ++i) {
            DependencyObject* lChild = ::Aero::LogicalTreeHelper::GetChild(*current, i);
            if (isDeferredToVisualDescendant(current, lChild)) continue;
            Base::Result<void> res = enqueue(current, lChild);
            if (!res) return res.GetStatus();
        }
    }
    return {};
}

Base::Result<void> EffectiveValueEngine::QueueDescendants(
    DependencyObject& parent,
    DependencyPropertyHandle property) noexcept {
    const DependencyProperty* registered = registry_->Find(property);
    if (registered != nullptr) {
        return PropagateToDescendants(
            parent,
            Base::Span<const DependencyProperty* const>(&registered, 1U),
            registered->Handle());
    }
    return PropagateToDescendants(parent, {}, property);
}

void EffectiveValueEngine::OnInheritancePropertyChanged(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    if (activePropagatingObject_ == &object &&
        activePropagatingProperty_ == property) {
        return;
    }
    static_cast<void>(QueueDescendants(object, property));
}


Base::Result<void> EffectiveValueEngine::Apply(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    const DependencyProperty* registered = registry_->Find(property);
    if (registered == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property is no longer registered");
    const PropertyMetadata* metadata = registered->MetadataFor(object.RuntimeType());
    if (metadata == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound,
        "Dependency property metadata is unavailable for the object");
    property = registered->Handle();
    PropertyValue inheritedValue;
    const PropertyValue* inherited = nullptr;
    if (HasFlag(metadata->flags, FrameworkPropertyMetadataOptions::Inherits)) {
        const std::uint64_t keyBloom =
            PropertyStore::Entries::KeyBloomBits(property.value);
        const bool checkDefaultOverride = registered->HasDerivedDefaultOverride();
        DependencyObject* inheritFrom = InheritanceParent(object);
        std::uint32_t loopCount = 0;
        while (inheritFrom != nullptr && ++loopCount <= 100U) {
            const PropertyStore* ancStore = inheritFrom->Store();
            const StoredValueEntry* ancEntry =
                (ancStore != nullptr && (ancStore->entries.bloomMask_ & keyBloom) == keyBloom)
                    ? ancStore->entries.Find(property.value)
                    : nullptr;
            if (ancEntry != nullptr || checkDefaultOverride) {
                if (const PropertyMetadata* ancMeta = registered->MetadataFor(inheritFrom->RuntimeType())) {
                    if (ancEntry != nullptr && ancEntry->HasNonDefaultState(ancMeta->defaultValue)) {
                        inherited = &ancEntry->effectiveValue;
                        break;
                    }
                    if (checkDefaultOverride && ancMeta->defaultValue != metadata->defaultValue) {
                        inherited = &ancMeta->defaultValue;
                        break;
                    }
                }
            }
            inheritFrom = InheritanceParent(*inheritFrom);
        }
        if (property == UIElement::IsEnabledProperty.Handle()) {
            if (const ::Aero::Media::Visual* visual =
                    ::Aero::TryCast<::Aero::Media::Visual>(&object)) {
                for (::Aero::Media::Visual* vParent = ::Aero::Media::VisualTreeHelper::GetParent(*visual);
                     vParent != nullptr;
                     vParent = ::Aero::Media::VisualTreeHelper::GetParent(*vParent)) {
                    if (const UIElement* parentElement = ::Aero::TryCast<UIElement>(vParent)) {
                        const PropertyValue* parentEnabled =
                            parentElement->FindEffectiveValuePtr(UIElement::IsEnabledProperty.Handle());
                        if (parentEnabled != nullptr &&
                            parentEnabled->Kind() == Meta::ValueKind::Boolean &&
                            !parentEnabled->AsBoolean()) {
                            inheritedValue = Meta::PropertyValue::FromBoolean(Meta::TypeOf<bool>(), false);
                            inherited = &inheritedValue;
                            break;
                        }
                    }
                }
            }
        }
    }

    PropertyStore* store = object.Store();
    StoredValueEntry* storedEntry = store != nullptr
        ? store->entries.Find(property.value)
        : nullptr;
    if (storedEntry == nullptr && inherited == nullptr) {
        return object.VerifyReady();
    }
    const PropertyValue oldEffective = storedEntry != nullptr
        ? storedEntry->effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = storedEntry != nullptr
        ? storedEntry->SourceInfo() : PropertyValueSourceInfo{};
    if (inherited != nullptr || (storedEntry != nullptr && storedEntry->HasInherited())) {
        Base::Result<void> stored =
            object.ApplyInheritedValueInternal(property, inherited);
        if (!stored) return stored.GetStatus();
    } else {
        Base::Result<void> ready = object.VerifyReady();
        if (!ready) return ready.GetStatus();
    }

    Base::Result<DependencyMutationScope> mutationResult =
        object.BeginMutation(property);
    if (!mutationResult) return mutationResult.GetStatus();
    DependencyMutationScope mutation = std::move(mutationResult).Value();
    return object.RecomputeEffectiveValueCore(
        property, *registered, *metadata, oldEffective, oldSourceInfo);
}

Base::Result<std::uint32_t> EffectiveValueEngine::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!initialized_) {
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
    const auto restoreRemainingQueueMasks = [this]() noexcept {
        for (QueueLink* cursor = queueHead_; cursor != nullptr; cursor = cursor->next) {
            if (cursor->object != nullptr) {
                cursor->object->unmaterializedQueueMask_ |= cursor->unmaterialized
                    ? UnmaterializedBitFor(cursor->inheritanceIndex)
                    : kHasQueuedStoredLinkBit;
            }
        }
    };
    std::uint32_t processed = 0U;
    while (queueHead_ != nullptr) {
        QueueLink* link = queueHead_;
        queueHead_ = link->next;
        if (queueHead_ == nullptr) {
            queueTail_ = nullptr;
        }
        --queueCount_;
        DependencyObject* object = link->object;
        DependencyPropertyHandle property = link->property;
        if (object == nullptr) {
            RecycleLink(link);
            continue;
        }
        if (!link->unmaterialized) {
            object->unmaterializedQueueMask_ &=
                static_cast<std::uint16_t>(~kHasQueuedStoredLinkBit);
        }
        PropertyStore* store = object->Store();
        StoredValueEntry* stored = store != nullptr
            ? store->entries.Find(property.value)
            : nullptr;
        if (stored == nullptr) {
            stored = object->FindStoredEntry(property);
        }
        bool descendantsQueued = link->descendantsQueued;
        if (stored != nullptr) {
            if (!stored->Queued()) {
                RecycleLink(link);
                continue;
            }
            descendantsQueued = descendantsQueued || stored->DescendantsQueued();
            stored->SetQueued(false);
            stored->SetDescendantsQueued(false);
        } else if (!link->unmaterialized) {
            RecycleLink(link);
            continue;
        }
        if (link->unmaterialized) {
            ClearUnmaterializedQueueBit(*object, link->inheritanceIndex);
        }
        DependencyObject* prevPropagatingObj = activePropagatingObject_;
        DependencyPropertyHandle prevPropagatingProp = activePropagatingProperty_;
        if (descendantsQueued) {
            activePropagatingObject_ = object;
            activePropagatingProperty_ = property;
        }
        Base::Result<void> applied = Apply(*object, property);
        activePropagatingObject_ = prevPropagatingObj;
        activePropagatingProperty_ = prevPropagatingProp;
        if (!applied) {
            if (stored != nullptr) {
                stored->SetQueued(true);
                stored->SetDescendantsQueued(descendantsQueued);
            }
            object->unmaterializedQueueMask_ |= link->unmaterialized
                ? UnmaterializedBitFor(link->inheritanceIndex)
                : kHasQueuedStoredLinkBit;
            link->object = object;
            link->property = property;
            link->descendantsQueued = descendantsQueued;
            link->next = queueHead_;
            queueHead_ = link;
            if (queueTail_ == nullptr) {
                queueTail_ = link;
            }
            ++queueCount_;
            restoreRemainingQueueMasks();
            return applied.GetStatus();
        }
        RecycleLink(link);
        ++processed;
        if (processed >= 65536U) {
            restoreRemainingQueueMasks();
            break;
        }
    }
    return processed;
}

Base::Result<void> EffectiveValueEngine::FlushProperty(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    if (!initialized_ || flushing_) {
        return {};
    }
    const DependencyProperty* registered = registry_ != nullptr
        ? registry_->Find(property)
        : nullptr;
    const DependencyPropertyHandle canonical = registered != nullptr
        ? registered->Handle()
        : DependencyPropertyHandle{object.CanonicalPropertyKey(property)};
    StoredValueEntry* stored = object.FindStoredEntry(canonical);
    if (stored != nullptr) {
        if (!stored->Queued()) {
            return {};
        }
        FlushScope scope(flushing_);
        const bool descendantsAlreadyQueued = stored->DescendantsQueued();
        stored->SetQueued(false);
        stored->SetDescendantsQueued(false);
        DependencyObject* prevPropagatingObj = activePropagatingObject_;
        DependencyPropertyHandle prevPropagatingProp = activePropagatingProperty_;
        if (descendantsAlreadyQueued) {
            activePropagatingObject_ = &object;
            activePropagatingProperty_ = canonical;
        }
        Base::Result<void> applied = Apply(object, canonical);
        activePropagatingObject_ = prevPropagatingObj;
        activePropagatingProperty_ = prevPropagatingProp;
        if (!applied) {
            stored->SetQueued(true);
            stored->SetDescendantsQueued(descendantsAlreadyQueued);
            return applied.GetStatus();
        }
        return {};
    }
    if ((object.unmaterializedQueueMask_ & kUnmaterializedBitsMask) == 0U) {
        return {};
    }
    if (registered != nullptr && registered->InheritanceIndex() < kMaxDirectInheritanceBits) {
        const std::uint16_t bit = UnmaterializedBitFor(registered->InheritanceIndex());
        if ((object.unmaterializedQueueMask_ & bit) == 0U) {
            return {};
        }
    }
    QueueLink* previous = nullptr;
    QueueLink* cursor = queueHead_;
    while (cursor != nullptr) {
        if (cursor->object == &object && cursor->property == canonical) {
            break;
        }
        previous = cursor;
        cursor = cursor->next;
    }
    if (cursor == nullptr) {
        return {};
    }
    if (previous == nullptr) {
        queueHead_ = cursor->next;
    } else {
        previous->next = cursor->next;
    }
    if (cursor == queueTail_) {
        queueTail_ = previous;
    }
    --queueCount_;
    const bool descendantsAlreadyQueued = cursor->descendantsQueued;
    RecycleLink(cursor);
    FlushScope scope(flushing_);
    DependencyObject* prevPropagatingObj = activePropagatingObject_;
    DependencyPropertyHandle prevPropagatingProp = activePropagatingProperty_;
    if (descendantsAlreadyQueued) {
        activePropagatingObject_ = &object;
        activePropagatingProperty_ = canonical;
    }
    Base::Result<void> applied = Apply(object, canonical);
    activePropagatingObject_ = prevPropagatingObj;
    activePropagatingProperty_ = prevPropagatingProp;
    return applied;
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
    Base::Result<void> verified = ResolveAndVerifyObjectProperty(object, property);
    if (!verified) return verified.GetStatus();
    Base::Result<void> applied =
        object.ApplyProviderContributionInternal(property, token, value);
    if (!applied) return applied.GetStatus();
    PropertyStore* store = object.Store();
    StoredValueEntry* stored = store != nullptr
        ? store->entries.Find(property.value)
        : nullptr;
    if (stored == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Dependency property entry missing after provider contribution");
    }
    return EnqueueStoredEntry(object, property, *stored);
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
    StoredValueEntry* stored = object.FindStoredEntry(property);
    if (stored == nullptr) return false;
    if (!object.VerifyReady() || !object.VerifyMutationAllowed()) return false;
    if (!stored->RemoveProviderToken(token)) return false;
    stored->ClearCurrent();
    const DependencyPropertyHandle canonical{object.CanonicalPropertyKey(property)};
    Base::Result<void> queued = EnqueueStoredEntry(object, canonical, *stored);
    if (!queued) return queued.GetStatus();
    return true;
}

Base::Result<std::uint32_t> EffectiveValueEngine::ClearProviderOrigin(
    DependencyObject& object, std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (origin == 0U) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property provider origin must be nonzero");
    PropertyStore* store = object.Store();
    if (store == nullptr || store->entries.Empty()) return 0U;
    if (!object.VerifyReady() || !object.VerifyMutationAllowed()) return 0U;
    std::uint32_t removed = 0U;
    for (PropertyStoreEntry& record : store->entries) {
        StoredValueEntry& stored = record.Value();
        if (stored.RemoveProviderOrigin(origin) == 0U) {
            continue;
        }
        stored.ClearCurrent();
        ++removed;
        Base::Result<void> queued = EnqueueStoredEntry(
            object, DependencyPropertyHandle{record.Key()}, stored);
        if (!queued) return queued.GetStatus();
    }
    return removed;
}

Base::Result<bool> EffectiveValueEngine::ClearProviderOrigin(
    DependencyObject& object,
    DependencyPropertyHandle property,
    std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    if (origin == 0U) return false;
    StoredValueEntry* stored = object.FindStoredEntry(property);
    if (stored == nullptr || !object.VerifyReady() || !object.VerifyMutationAllowed()) {
        return false;
    }
    if (stored->RemoveProviderOrigin(origin) == 0U) {
        return false;
    }
    stored->ClearCurrent();
    const DependencyPropertyHandle canonical{object.CanonicalPropertyKey(property)};
    Base::Result<void> queued = EnqueueStoredEntry(object, canonical, *stored);
    if (!queued) return queued.GetStatus();
    return true;
}

Base::Result<void> EffectiveValueEngine::SetLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<void> verified = ResolveAndVerifyObjectProperty(object, property);
    if (!verified) return verified.GetStatus();
    Base::Result<void> applied =
        object.ApplyLocalExpressionInternal(property, expression);
    if (!applied) return applied.GetStatus();
    PropertyStore* store = object.Store();
    StoredValueEntry* stored = store != nullptr
        ? store->entries.Find(property.value)
        : nullptr;
    if (stored == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Dependency property entry missing after expression assignment");
    }
    return EnqueueStoredEntry(object, property, *stored);
}

Base::Result<void> EffectiveValueEngine::ClearLocalExpression(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    StoredValueEntry* stored = object.FindStoredEntry(property);
    if (stored == nullptr || !stored->HasExpression()) return {};
    if (!object.VerifyReady() || !object.VerifyMutationAllowed()) return {};
    const DependencyPropertyHandle canonical{object.CanonicalPropertyKey(property)};
    Base::Result<void> queued = EnqueueStoredEntry(object, canonical, *stored);
    if (!queued) return queued.GetStatus();
    stored->ClearCurrent();
    object.ReleaseExpression(*stored);
    return {};
}

Base::Result<void> EffectiveValueEngine::SetAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    Base::Result<void> verified = ResolveAndVerifyObjectProperty(object, property);
    if (!verified) return verified.GetStatus();
    Base::Result<void> applied =
        object.ApplyAnimationValueInternal(property, value);
    if (!applied) return applied.GetStatus();
    PropertyStore* store = object.Store();
    StoredValueEntry* stored = store != nullptr
        ? store->entries.Find(property.value)
        : nullptr;
    if (stored == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Dependency property entry missing after animation assignment");
    }
    return EnqueueStoredEntry(object, property, *stored);
}

Base::Result<void> EffectiveValueEngine::ClearAnimationValue(
    DependencyObject& object, DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    StoredValueEntry* stored = object.FindStoredEntry(property);
    if (stored == nullptr || !stored->HasAnimation()) return {};
    if (!object.VerifyReady() || !object.VerifyMutationAllowed()) return {};
    stored->ClearAnimation();
    const DependencyPropertyHandle canonical{object.CanonicalPropertyKey(property)};
    return EnqueueStoredEntry(object, canonical, *stored);
}

Base::Result<void> EffectiveValueEngine::Invalidate(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) return ready.GetStatus();
    Base::Result<StoredValueEntry*> ensured =
        EnsureAndQueueProperty(object, property);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry& stored = *ensured.Value();
    if (stored.HasCurrent() && object.VerifyReady() && object.VerifyMutationAllowed()) {
        stored.ClearCurrent();
    }
    return QueueDescendants(object, property);
}

void EffectiveValueEngine::OnObjectDestroyed(DependencyObject& object) noexcept {
    if (object.StyleApplicationIndex() != UINT32_MAX && styleDestroyHook_ != nullptr) {
        styleDestroyHook_(styleDestroyContext_, object);
    }
    if (queueHead_ != nullptr && (flushing_ || object.unmaterializedQueueMask_ != 0U)) {
        PropertyStore* store = object.Store();
        QueueLink* previous = nullptr;
        QueueLink* cursor = queueHead_;
        while (cursor != nullptr) {
            QueueLink* next = cursor->next;
            if (cursor->object == &object) {
                if (!cursor->unmaterialized && store != nullptr) {
                    if (StoredValueEntry* stored = store->entries.Find(cursor->property.value)) {
                        stored->SetQueued(false);
                        stored->SetDescendantsQueued(false);
                    }
                }
                if (previous == nullptr) {
                    queueHead_ = next;
                } else {
                    previous->next = next;
                }
                if (cursor == queueTail_) {
                    queueTail_ = previous;
                }
                --queueCount_;
                RecycleLink(cursor);
            } else {
                previous = cursor;
            }
            cursor = next;
        }
    }
    object.unmaterializedQueueMask_ = 0U;
    if (DependencyObject* previousParent = object.ExplicitInheritanceParent()) {
        UnregisterExplicitInheritanceChild(previousParent, object);
        object.SetExplicitInheritanceParent(nullptr);
    }
    if (object.hasExplicitInheritanceChildren_ && object.rare_ != nullptr) {
        DependencyObject* child = object.rare_->firstInheritanceChild;
        object.rare_->firstInheritanceChild = nullptr;
        while (child != nullptr) {
            DependencyObject* nextChild = child->rare_ != nullptr
                ? child->rare_->nextInheritanceSibling
                : nullptr;
            if (child->rare_ != nullptr) {
                child->rare_->nextInheritanceSibling = nullptr;
            }
            child->SetExplicitInheritanceParent(nullptr);
            child = nextChild;
        }
        DependencyObject* prevParent = nullptr;
        DependencyObject* curParent = firstExplicitParent_;
        while (curParent != nullptr) {
            if (curParent == &object) {
                if (prevParent != nullptr) {
                    prevParent->rare_->nextExplicitParent =
                        object.rare_->nextExplicitParent;
                } else {
                    firstExplicitParent_ = object.rare_->nextExplicitParent;
                }
                object.rare_->nextExplicitParent = nullptr;
                break;
            }
            prevParent = curParent;
            curParent = curParent->rare_ != nullptr
                ? curParent->rare_->nextExplicitParent
                : nullptr;
        }
        object.hasExplicitInheritanceChildren_ = false;
        object.CleanupRareIfEmpty();
    }
}

bool EffectiveValueEngine::HasObjectState(
    const DependencyObject& object) const noexcept {
    if (object.unmaterializedQueueMask_ != 0U) {
        return true;
    }
    if (object.hasExplicitInheritanceParent_ || object.hasExplicitInheritanceChildren_) {
        return true;
    }
    const PropertyStore* store = object.Store();
    if (store == nullptr || store->entries.Empty()) {
        return false;
    }
    for (const auto& record : store->entries) {
        if (record.Value().HasEngineState()) {
            return true;
        }
    }
    return false;
}

Base::Result<void> EffectiveValueEngine::DetachObject(DependencyObject& object) noexcept {
    Base::Result<void> ready = VerifyMutable(); if (!ready) return ready.GetStatus();
    OnObjectDestroyed(object);
    return object.DropAllEngineValueState();
}

std::uint32_t EffectiveValueEngine::PendingPropertyCount() const noexcept {
    // P2.2: every linked node was enqueued with Queued() set and the flag is
    // only cleared on dequeue, so the counter is exact.
    return queueCount_;
}

} // namespace Aero::Meta

namespace Aero {

void DependencyObject::CopyStoredKeys(Base::Vector<MemberId>& outKeys) const noexcept {
    const PropertyStore* store = Store();
    if (store == nullptr) return;
    outKeys.Reserve(outKeys.Size() + store->entries.Size());
    for (const auto& record : store->entries) {
        outKeys.PushBack(record.Key());
    }
}

void DependencyObject::ForEachStoredKey(
    void (*visitor)(void*, MemberId) noexcept,
    void* context) noexcept {
    PropertyStore* store = Store();
    if (store == nullptr || visitor == nullptr) return;
    for (auto& record : store->entries) {
        visitor(context, record.Key());
    }
}

std::uint32_t DependencyObject::ClearAllProviderOrigin(
    std::uint32_t origin) noexcept {
    PropertyStore* store = Store();
    if (store == nullptr || store->entries.Empty()) return 0U;
    if (!VerifyReady() || !VerifyMutationAllowed()) return 0U;
    std::uint32_t removed = 0U;
    for (PropertyStoreEntry& record : store->entries) {
        StoredValueEntry& stored = record.Value();
        if (stored.RemoveProviderOrigin(origin) != 0U) {
            stored.ClearCurrent();
            ++removed;
        }
    }
    return removed;
}

Base::Result<void> DependencyObject::DropAllEngineValueState() noexcept {
    PropertyStore* store = Store();
    if (store == nullptr || store->entries.Empty()) {
        return {};
    }
    Base::BasicVector<MemberId, 16U> keys;
    for (const auto& record : store->entries) {
        if (record.Value().HasEngineState()) {
            keys.PushBack(record.Key());
        }
    }
    for (MemberId key : keys) {
        Base::Result<void> dropped =
            DropEngineValueStateInternal(DependencyPropertyHandle{key});
        if (!dropped) return dropped.GetStatus();
    }
    return {};
}

void DependencyObject::DetachPropertyDependencyObjects(
    BindingEngine* bindings,
    Meta::EffectiveValueEngine* values,
    AnimationEngine* animations,
    Base::HashSet<DependencyObject*>& visited) noexcept {
    DependencyObject& object = *this;
    const auto* selfVisual = ::Aero::TryCast<Aero::Media::Visual>(&object);
    const bool isVisual = selfVisual != nullptr;
    const bool alreadyVisited = !visited.Insert(&object).inserted;
    if (alreadyVisited && !isVisual) {
        return;
    }

    const auto resolveItemsSource = [](Base::Object* target) noexcept -> Collections::IItemsSource* {
        if (target == nullptr) return nullptr;
        if (auto* items = ::Aero::TryCastToInterface<Collections::IItemsSource>(target)) {
            return items;
        }
        return Collections::CollectionAsItemsSource(target);
    };

    const auto anyItemDependencyObject = [&](Base::Object* container, auto&& predicate) noexcept -> bool {
        Collections::IItemsSource* itemsSource = resolveItemsSource(container);
        if (itemsSource == nullptr) return false;
        const std::uint32_t count = itemsSource->GetCount();
        for (std::uint32_t i = 0U; i < count; ++i) {
            Base::Ref<Base::Object> item = itemsSource->GetItem(i);
            if (item) {
                if (auto* itemDO = ::Aero::TryCast<DependencyObject>(item.Get())) {
                    if (predicate(itemDO)) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const auto hasDetachableTreeState = [&](const auto& self,
                                            const DependencyObject& candidate,
                                            std::uint32_t depth) noexcept -> bool {
        if (animations != nullptr && animations->HasTarget(candidate)) {
            return true;
        }
        if (bindings != nullptr && bindings->HasAnyObjectReference(candidate)) {
            return true;
        }
        if (ElementTree::HasAnyBindingOnDispatcher(
                candidate.GetDispatcher(), candidate, bindings)) {
            return true;
        }
        if (values != nullptr && values->HasObjectState(candidate)) {
            return true;
        }
        if (depth >= 8U) {
            return false;
        }
        const PropertyStore* candStore = candidate.Store();
        if (candStore != nullptr) {
            const auto checkNestedValue = [&](const Meta::PropertyValue& val) noexcept -> bool {
                if (val.Kind() != Base::ValueKind::Object || val.IsNullObject()) {
                    return false;
                }
                const Base::Ref<Base::Object>& ref = val.AsObject();
                if (!ref) return false;
                if (const auto* depObj = ::Aero::TryCast<DependencyObject>(ref.Get())) {
                    if (depObj != &candidate &&
                        ::Aero::TryCast<Aero::Media::Visual>(depObj) == nullptr &&
                        self(self, *depObj, depth + 1U)) {
                        return true;
                    }
                } else if (ref->UseCount() <= 1U) {
                    if (anyItemDependencyObject(ref.Get(), [&](DependencyObject* itemDO) noexcept {
                            return ::Aero::TryCast<Aero::Media::Visual>(itemDO) == nullptr &&
                                self(self, *itemDO, depth + 1U);
                        })) {
                        return true;
                    }
                }
                return false;
            };
            for (const auto& entry : candStore->entries) {
                if (!entry.Value().SourceInfo().isInherited &&
                    checkNestedValue(entry.Value().effectiveValue)) {
                    return true;
                }
                if (entry.Value().rare != nullptr) {
                    if (checkNestedValue(entry.Value().rare->localBackup) ||
                        checkNestedValue(entry.Value().rare->currentValue) ||
                        checkNestedValue(entry.Value().rare->animationValue)) {
                        return true;
                    }
                }
            }
        }
        return anyItemDependencyObject(
            const_cast<DependencyObject*>(&candidate),
            [&](DependencyObject* itemDO) noexcept {
                return ::Aero::TryCast<Aero::Media::Visual>(itemDO) == nullptr &&
                    self(self, *itemDO, depth + 1U);
            });
    };

    const auto isSharedOutside = [&](DependencyObject* candidate, std::uint32_t expectedRefs) noexcept {
        if (candidate == nullptr) return false;
        const auto* freezable = ::Aero::TryCast<Freezable>(candidate);
        if (freezable != nullptr) {
            if (freezable->IsFrozen() || freezable->HasActiveConsumersOutside(visited)) {
                return true;
            }
        }
        if (bindings != nullptr && bindings->HasActiveBindingsOutside(*candidate, visited)) {
            return true;
        }
        if (freezable == nullptr && candidate->UseCount() > expectedRefs) {
            return true;
        }
        if (ElementTree::HasActiveVisualOrBindingOnDispatcher(
                object.GetDispatcher(), *candidate, visited, bindings)) {
            return true;
        }
        return false;
    };

    PropertyStore* store = (object).Store();
    if (store != nullptr) {
        struct PropertyObjectRef {
            DependencyObject* object = nullptr;
            std::uint32_t count = 0U;
        };
        Base::BasicVector<PropertyObjectRef, 8U> propertyObjects;

        const auto recordPropertyObject = [&](DependencyObject* depObj) noexcept {
            if (depObj == nullptr || depObj == &object ||
                ::Aero::TryCast<Aero::Media::Visual>(depObj) != nullptr) {
                return;
            }
            for (PropertyObjectRef& existing : propertyObjects) {
                if (existing.object == depObj) {
                    ++existing.count;
                    return;
                }
            }
            propertyObjects.PushBack(PropertyObjectRef{depObj, 1U});
        };

        const auto inspectValue = [&](const Meta::PropertyValue& val) noexcept {
            if (val.Kind() == Base::ValueKind::Object && !val.IsNullObject()) {
                const Base::Ref<Base::Object>& ref = val.AsObject();
                if (ref) {
                    if (auto* depObj = ::Aero::TryCast<DependencyObject>(ref.Get())) {
                        recordPropertyObject(depObj);
                    } else if (ref->UseCount() <= 1U) {
                        static_cast<void>(anyItemDependencyObject(ref.Get(), [&](DependencyObject* itemDO) noexcept {
                            recordPropertyObject(itemDO);
                            return false;
                        }));
                    }
                }
            }
        };

        for (auto& entry : store->entries) {
            if (!entry.Value().SourceInfo().isInherited) {
                inspectValue(entry.Value().effectiveValue);
            }
            if (entry.Value().rare != nullptr) {
                inspectValue(entry.Value().rare->localBackup);
                inspectValue(entry.Value().rare->currentValue);
                inspectValue(entry.Value().rare->animationValue);
            }
        }

        for (const PropertyObjectRef& propRef : propertyObjects) {
            DependencyObject* child = propRef.object;
            if (child == nullptr) continue;
            if (!hasDetachableTreeState(hasDetachableTreeState, *child, 0U)) {
                continue;
            }
            const std::uint32_t expectedRefs = propRef.count > 0U ? propRef.count : 1U;
            if (isSharedOutside(child, expectedRefs)) {
                continue;
            }
            static_cast<void>(anyItemDependencyObject(child, [&](DependencyObject* itemDO) noexcept {
                if (::Aero::TryCast<Aero::Media::Visual>(itemDO) == nullptr &&
                    hasDetachableTreeState(hasDetachableTreeState, *itemDO, 0U) &&
                    !isSharedOutside(itemDO, 2U)) {
                    itemDO->DetachPropertyDependencyObjects(bindings, values, animations, visited);
                }
                return false;
            }));
            child->DetachPropertyDependencyObjects(bindings, values, animations, visited);
        }
    }

    if (!isVisual) {
        if (animations != nullptr) animations->RemoveTarget(object);
        if (bindings != nullptr) (void)bindings->DetachObject(object);
        ElementTree::DetachObjectFromAllBindingsOnDispatcher(object.GetDispatcher(), object, bindings);
        if (values != nullptr) (void)values->DetachObject(object);
    }
}

} // namespace Aero

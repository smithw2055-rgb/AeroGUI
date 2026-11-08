// Consolidated metadata implementation. Keep sections ordered by dependency.

#include <Aero/Value.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <utility>

// ===== TypeRegistry =====




namespace Aero::Meta {
namespace {

struct CompactTypeIdTable {
    static constexpr std::uint32_t kMaxTypes = 65536U;
    static constexpr std::uint32_t kHashCapacity = 131072U;
    static constexpr std::uint32_t kHashMask = kHashCapacity - 1U;

    std::atomic<std::uint32_t> count{1U};
    TypeId entries[kMaxTypes]{};
    std::atomic<std::uint16_t> hashTable[kHashCapacity]{};
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
};

CompactTypeIdTable g_compactTypeIdTable{};

struct RuntimeTypeBinding {
    TypeId id = InvalidTypeId;
    TypeId baseType = InvalidTypeId;
    MetadataTypeKind kind = MetadataTypeKind::Struct;
    Base::String xamlNamespace;
    Base::String name;
};

Base::HashMap<TypeId, RuntimeTypeBinding>& RuntimeTypeBindings() noexcept {
    static Base::HashMap<TypeId, RuntimeTypeBinding> bindings;
    return bindings;
}

bool SameRuntimeTypeInfo(
    const RuntimeTypeBinding& binding,
    const RuntimeTypeInfo& info) noexcept {
    return binding.id == info.id &&
        binding.baseType == info.baseType &&
        binding.kind == info.kind &&
        binding.xamlNamespace == info.xamlNamespace &&
        binding.name == info.name;
}

constexpr Base::Status EmptyTypeNameStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Type namespace and name must be non-empty UTF-8 strings");
}

} // namespace

std::uint16_t InternCompactTypeId(TypeId type) noexcept {
    if (type == InvalidTypeId) {
        return 0U;
    }
    const std::uint32_t h = static_cast<std::uint32_t>(type ^ (type >> 32U)) &
        CompactTypeIdTable::kHashMask;
    for (std::uint32_t step = 0U; step < CompactTypeIdTable::kHashCapacity; ++step) {
        const std::uint32_t bucket = (h + step) & CompactTypeIdTable::kHashMask;
        const std::uint16_t idx =
            g_compactTypeIdTable.hashTable[bucket].load(std::memory_order_acquire);
        if (idx == 0U) {
            break;
        }
        if (g_compactTypeIdTable.entries[idx] == type) {
            return idx;
        }
    }

    while (g_compactTypeIdTable.lock.test_and_set(std::memory_order_acquire)) {
    }
    std::uint16_t assigned = 0U;
    for (std::uint32_t step = 0U; step < CompactTypeIdTable::kHashCapacity; ++step) {
        const std::uint32_t bucket = (h + step) & CompactTypeIdTable::kHashMask;
        const std::uint16_t idx =
            g_compactTypeIdTable.hashTable[bucket].load(std::memory_order_relaxed);
        if (idx == 0U) {
            const std::uint32_t next =
                g_compactTypeIdTable.count.fetch_add(1U, std::memory_order_relaxed);
            AERO_ASSERT(next < CompactTypeIdTable::kMaxTypes);
            assigned = static_cast<std::uint16_t>(next);
            g_compactTypeIdTable.entries[assigned] = type;
            g_compactTypeIdTable.hashTable[bucket].store(assigned, std::memory_order_release);
            break;
        }
        if (g_compactTypeIdTable.entries[idx] == type) {
            assigned = idx;
            break;
        }
    }
    g_compactTypeIdTable.lock.clear(std::memory_order_release);
    return assigned;
}

TypeId ResolveCompactTypeId(std::uint16_t index) noexcept {
    return g_compactTypeIdTable.entries[index];
}

Base::Status BindRuntimeTypeInfo(
    TypeId token,
    const RuntimeTypeInfo& info) noexcept {
    if (token == InvalidTypeId ||
        info.id == InvalidTypeId ||
        info.xamlNamespace.Empty() ||
        info.name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Runtime metadata type binding is incomplete");
    }

    auto& bindings = RuntimeTypeBindings();
    if (const RuntimeTypeBinding* existing = bindings.Find(token)) {
        return SameRuntimeTypeInfo(*existing, info)
            ? Base::Status::Ok()
            : Base::Status::Failure(
                Base::ErrorCode::IdCollision,
                "C++ type is already bound to a different metadata type");
    }

    RuntimeTypeBinding binding;
    binding.id = info.id;
    binding.baseType = info.baseType;
    binding.kind = info.kind;
    Base::Result<void> copiedNamespace =
        binding.xamlNamespace.Assign(info.xamlNamespace);
    if (!copiedNamespace) return copiedNamespace.GetStatus();
    Base::Result<void> copiedName = binding.name.Assign(info.name);
    if (!copiedName) return copiedName.GetStatus();

    bindings.Insert(token, std::move(binding));
    return Base::Status::Ok();
}

RuntimeTypeInfo ResolveRuntimeTypeInfo(TypeId token) noexcept {
    RuntimeTypeInfo result;
    const RuntimeTypeBinding* binding =
        RuntimeTypeBindings().Find(token);
    if (binding == nullptr) return result;
    result.id = binding->id;
    result.xamlNamespace = binding->xamlNamespace.View();
    result.name = binding->name.View();
    result.baseType = binding->baseType;
    result.kind = binding->kind;
    return result;
}

namespace {

constexpr Base::Status EmptyMemberNameStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Member name must be a non-empty UTF-8 string");
}

constexpr Base::Status RegistryFrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "TypeRegistry is frozen");
}

constexpr Base::Status DuplicateTypeStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::AlreadyExists,
        "Type is already registered");
}

constexpr Base::Status DuplicateMemberStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::AlreadyExists,
        "Member is already registered");
}

constexpr Base::Status IdCollisionStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::IdCollision,
        "Stable metadata ID collision detected");
}

constexpr Base::Status MissingOwnerStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Member owner type is not registered");
}

constexpr Base::Status MissingRelatedTypeStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Related metadata type is not registered");
}

constexpr Base::Status InheritanceCycleStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::CycleDetected,
        "Type inheritance or interface cycle detected");
}

bool HasPropertyBehavior(
    const PropertyRegistration& registration) noexcept {
    return registration.access != PropertyAccessKind::External ||
        registration.get != nullptr || registration.set != nullptr ||
        registration.provider != InvalidPropertyProviderId ||
        registration.context != nullptr;
}

bool IsValidPropertyBehavior(
    PropertyAccessKind access,
    PropertyGetCallback get,
    PropertySetCallback set,
    PropertyProviderId provider,
    void* context) noexcept {
    switch (access) {
    case PropertyAccessKind::External:
        return get == nullptr && set == nullptr &&
            provider == InvalidPropertyProviderId && context == nullptr;
    case PropertyAccessKind::Ordinary:
        return (get != nullptr || set != nullptr) &&
            provider == InvalidPropertyProviderId;
    case PropertyAccessKind::Provider:
        return provider != InvalidPropertyProviderId;
    }
    return false;
}

bool InterfaceReachable(
    const TypeRegistry& registry,
    TypeId current,
    TypeId target,
    std::uint32_t depth) noexcept {
    if (current == target) return true;
    if (depth > registry.TypeCount()) return false;
    const TypeInfo* info = registry.FindType(current);
    if (info == nullptr) return false;
    for (TypeId direct : info->Interfaces()) {
        if (InterfaceReachable(registry, direct, target, depth + 1U)) {
            return true;
        }
    }
    return false;
}

bool TypeImplements(
    const TypeRegistry& registry,
    TypeId current,
    TypeId target,
    std::uint32_t depth) noexcept {
    if (current == InvalidTypeId || depth > registry.TypeCount()) return false;
    const TypeInfo* owner = registry.FindType(current);
    if (owner == nullptr) return false;
    if (registry.IsFrozen() && !owner->AncestorIndices().Empty()) {
        const Base::Span<const TypeInfo> allTypes = registry.Types();
        for (std::uint32_t ancestorIndex : owner->AncestorIndices()) {
            if (ancestorIndex >= allTypes.Size()) break;
            const TypeInfo& info = allTypes[ancestorIndex];
            for (TypeId direct : info.Interfaces()) {
                if (direct == target ||
                    InterfaceReachable(registry, direct, target, 1U)) {
                    return true;
                }
            }
        }
        return false;
    }
    for (TypeId direct : owner->Interfaces()) {
        if (direct == target ||
            InterfaceReachable(registry, direct, target, depth + 1U)) {
            return true;
        }
    }
    return TypeImplements(
        registry, owner->BaseType(), target, depth + 1U);
}

template<class T>
Base::Result<void> SortInfoById(
    Base::Vector<const T*>& values) noexcept {
    for (std::uint32_t index = 1U; index < values.Size(); ++index) {
        const T* value = values[index];
        std::uint32_t cursor = index;
        while (cursor > 0U &&
               value->Id() < values[cursor - 1U]->Id()) {
            values[cursor] = values[cursor - 1U];
            --cursor;
        }
        values[cursor] = value;
    }
    return {};
}

} // namespace

MemberId MakeMethodId(
    TypeId ownerType,
    Base::StringView name,
    Base::Span<const TypeId> parameterTypes) noexcept {
    static constexpr char Domain[] = "AERO.METHOD.V1";
    Base::StableMetadataIdBuilder builder;
    builder.AddText(
        Domain, static_cast<std::uint32_t>(sizeof(Domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddString(name);
    builder.AddU32(parameterTypes.Size());
    for (TypeId parameterType : parameterTypes) {
        builder.AddU64(parameterType);
    }
    return builder.Finish();
}

TypeRegistry::TypeRegistry() noexcept
    : types_(),
      typeIndex_(),
      memberIndex_() {}

Base::Result<TypeId> TypeRegistry::RegisterType(
    BehaviorTable& behaviors,
    const TypeRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.xamlNamespace.Empty() || registration.name.Empty()) {
        return EmptyTypeNameStatus();
    }

    const TypeId id = MakeTypeId(
        registration.xamlNamespace, registration.name);
    const std::uint32_t* existingIndex = typeIndex_.Find(id);
    if (existingIndex != nullptr) {
        const TypeInfo& existing = types_[*existingIndex];
        return existing.XamlNamespace() == registration.xamlNamespace &&
            existing.Name() == registration.name
            ? Base::Result<TypeId>(DuplicateTypeStatus())
            : Base::Result<TypeId>(IdCollisionStatus());
    }

    const MetadataTypeKind kind = registration.kind;
    TypeFlags flags = registration.flags;
    if (kind == MetadataTypeKind::Interface) {
        flags = flags | TypeFlags::Abstract;
    } else if (kind == MetadataTypeKind::Struct ||
               kind == MetadataTypeKind::Enum ||
               kind == MetadataTypeKind::Primitive) {
        flags = flags | TypeFlags::ValueType | TypeFlags::Sealed;
    }
    if (registration.factory != nullptr &&
        kind != MetadataTypeKind::Object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Only object metadata types may register factories");
    }

    if (registration.factory != nullptr) {
        behaviors.typeFactories_.Reserve(
            behaviors.typeFactories_.Size() + 1U);
    }

    TypeInfo info;
    info.id_ = id;
    info.baseType_ = registration.baseType;
    info.underlyingType_ = registration.underlyingType;
    info.kind_ = kind;
    info.flags_ = flags;
    Base::Result<void> result =
        info.xamlNamespace_.Assign(registration.xamlNamespace);
    if (!result) return result.GetStatus();
    result = info.name_.Assign(registration.name);
    if (!result) return result.GetStatus();
    if (!registration.interfaces.Empty()) {
        TypeRareData* rare = info.EnsureRare();
        if (rare == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Failed to allocate type rare data");
        }
        rare->interfaces.Reserve(registration.interfaces.Size());
        for (TypeId interfaceType : registration.interfaces) {
            if (interfaceType == InvalidTypeId) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Implemented interface type must be valid");
            }
            for (TypeId existing : rare->interfaces) {
                if (existing == interfaceType) return DuplicateMemberStatus();
            }
            rare->interfaces.PushBack(interfaceType);
            rare->interfaceCasts.PushBack(nullptr);
        }
    }

    const std::uint32_t index = types_.Size();
    types_.PushBack(std::move(info));
    bool factoryAdded = false;
    if (registration.factory != nullptr) {
        const std::uint32_t factoryIdx = behaviors.typeFactories_.Size();
        behaviors.typeFactories_.PushBack({id, registration.factory});
        static_cast<void>(behaviors.typeFactoriesIndex_.Insert(id, factoryIdx));
        factoryAdded = true;
    }

    Base::HashMap<TypeId, std::uint32_t>::InsertResult inserted =
        typeIndex_.Insert(id, index);
    if (!inserted.inserted) {
        if (factoryAdded) {
            behaviors.typeFactoriesIndex_.Erase(id);
            behaviors.typeFactories_.PopBack();
        }
        types_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::RegisterInterface(
    TypeId ownerType,
    TypeId interfaceType,
    InterfaceCastThunk cast) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (interfaceType == InvalidTypeId || ownerType == interfaceType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implemented interface relation is invalid");
    }
    TypeInfo* owner = MutableType(ownerType);
    if (owner == nullptr) return MissingOwnerStatus();
    TypeRareData* rare = owner->EnsureRare();
    if (rare == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Failed to allocate type rare data");
    }
    for (TypeId existing : rare->interfaces) {
        if (existing == interfaceType) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Implemented interface is already registered");
        }
    }
    while (rare->interfaceCasts.Size() < rare->interfaces.Size()) {
        rare->interfaceCasts.PushBack(nullptr);
    }
    rare->interfaces.PushBack(interfaceType);
    rare->interfaceCasts.PushBack(cast);
    return {};
}

Base::Result<MemberId> TypeRegistry::RegisterProperty(
    BehaviorTable& behaviors,
    TypeId ownerType,
    const PropertyRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.name.Empty()) return EmptyMemberNameStatus();
    if (registration.valueType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property value type must be valid");
    }
    if (!IsValidPropertyBehavior(
            registration.access,
            registration.get,
            registration.set,
            registration.provider,
            registration.context)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property access metadata is invalid");
    }

    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::Property, registration.name);
    const MemberLocation* existingLocation = memberIndex_.Find(id);
    if (existingLocation != nullptr) {
        const PropertyInfo* existing = PropertyAt(*existingLocation);
        return existing != nullptr && existing->OwnerType() == ownerType &&
            existing->Name() == registration.name
            ? Base::Result<MemberId>(DuplicateMemberStatus())
            : Base::Result<MemberId>(IdCollisionStatus());
    }

    const bool hasBehavior = HasPropertyBehavior(registration);
    if (hasBehavior) {
        behaviors.propertyAccessors_.Reserve(
            behaviors.propertyAccessors_.Size() + 1U);
    }

    TypeInfo& owner = types_[*ownerIndex];
    PropertyInfo property;
    property.id_ = id;
    property.ownerType_ = ownerType;
    property.valueType_ = registration.valueType;
    property.flags_ = registration.flags;
    Base::Result<void> result = property.name_.Assign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t propertyIndex = owner.properties_.Size();
    owner.properties_.PushBack(std::move(property));
    bool behaviorAdded = false;
    if (hasBehavior) {
        const std::uint32_t propAccessorIdx = behaviors.propertyAccessors_.Size();
        behaviors.propertyAccessors_.PushBack({
            id, registration.access, registration.get, registration.set,
            registration.provider, registration.context});
        static_cast<void>(behaviors.propertyAccessorsIndex_.Insert(id, propAccessorIdx));
        behaviorAdded = true;
    }

    const MemberLocation location{
        *ownerIndex, propertyIndex, MemberKind::Property};
    Base::HashMap<MemberId, MemberLocation>::InsertResult inserted =
        memberIndex_.Insert(id, location);
    if (!inserted.inserted) {
        if (behaviorAdded) {
            behaviors.propertyAccessorsIndex_.Erase(id);
            behaviors.propertyAccessors_.PopBack();
        }
        owner.properties_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}


Base::Result<MemberId> TypeRegistry::RegisterEnumValue(
    TypeId ownerType,
    const EnumValueRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.name.Empty()) return EmptyMemberNameStatus();
    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();
    TypeInfo& owner = types_[*ownerIndex];
    if (owner.Kind() != MetadataTypeKind::Enum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Enum values may only be declared by enum metadata types");
    }

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::EnumValue, registration.name);
    if (memberIndex_.Find(id) != nullptr) return DuplicateMemberStatus();
    for (const EnumValueInfo& value : owner.enumValues_) {
        if (!owner.IsFlagsEnum() && value.RawValue() == registration.rawValue) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Non-flags enum raw value is already registered");
        }
    }

    EnumValueInfo value;
    value.id_ = id;
    value.ownerType_ = ownerType;
    value.rawValue_ = registration.rawValue;
    Base::Result<void> result = value.name_.Assign(registration.name);
    if (!result) return result.GetStatus();
    const std::uint32_t valueIndex = owner.enumValues_.Size();
    owner.enumValues_.PushBack(std::move(value));
    Base::HashMap<MemberId, MemberLocation>::InsertResult inserted =
        memberIndex_.Insert(
            id, {*ownerIndex, valueIndex, MemberKind::EnumValue});
    if (!inserted.inserted) {
        owner.enumValues_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::RegisterEvent(
    TypeId ownerType,
    const EventRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.name.Empty()) return EmptyMemberNameStatus();
    if (registration.eventArgsType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Event argument type must be valid");
    }

    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::Event, registration.name);
    const MemberLocation* existingLocation = memberIndex_.Find(id);
    if (existingLocation != nullptr) {
        const EventInfo* existing = EventAt(*existingLocation);
        return existing != nullptr && existing->OwnerType() == ownerType &&
            existing->Name() == registration.name
            ? Base::Result<MemberId>(DuplicateMemberStatus())
            : Base::Result<MemberId>(IdCollisionStatus());
    }

    TypeInfo& owner = types_[*ownerIndex];
    EventInfo eventInfo;
    eventInfo.id_ = id;
    eventInfo.ownerType_ = ownerType;
    eventInfo.eventArgsType_ = registration.eventArgsType;
    eventInfo.flags_ = registration.flags;
    Base::Result<void> result = eventInfo.name_.Assign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t eventIndex = owner.events_.Size();
    owner.events_.PushBack(std::move(eventInfo));
    const MemberLocation location{
        *ownerIndex, eventIndex, MemberKind::Event};
    Base::HashMap<MemberId, MemberLocation>::InsertResult inserted =
        memberIndex_.Insert(id, location);
    if (!inserted.inserted) {
        owner.events_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::RegisterEventHandler(
    TypeId ownerType,
    Base::StringView name,
    EventHandlerThunk thunk) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (name.Empty() || thunk == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Event handler name and thunk are required");
    }
    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();

    TypeInfo& owner = types_[*ownerIndex];
    TypeRareData* rare = owner.EnsureRare();
    if (rare == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Failed to allocate type rare data");
    }
    for (const EventHandlerDescriptor& existing : rare->eventHandlers) {
        if (existing.name == name) {
            return DuplicateMemberStatus();
        }
    }

    EventHandlerDescriptor descriptor;
    descriptor.thunk = thunk;
    Base::Result<void> result = descriptor.name.Assign(name);
    if (!result) return result.GetStatus();

    rare->eventHandlers.PushBack(std::move(descriptor));
    return {};
}

Base::Result<void> TypeRegistry::RegisterTemplatePart(
    TypeId ownerType,
    Base::StringView name,
    TypeId partType) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (name.Empty() || partType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template part name and part type are required");
    }
    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();

    TypeInfo& owner = types_[*ownerIndex];
    TypeRareData* rare = owner.EnsureRare();
    if (rare == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Failed to allocate type rare data");
    }
    for (const TemplatePartDescriptor& existing : rare->templateParts) {
        if (existing.name == name) {
            return DuplicateMemberStatus();
        }
    }

    TemplatePartDescriptor descriptor;
    descriptor.partType = partType;
    Base::Result<void> result = descriptor.name.Assign(name);
    if (!result) return result.GetStatus();

    rare->templateParts.PushBack(std::move(descriptor));
    return {};
}
Base::Result<void> TypeRegistry::SetFactory(
    BehaviorTable& behaviors,
    TypeId type,
    ObjectFactory factory) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    const TypeInfo* info = FindType(type);
    if (info == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Factory owner type was not found");
    }
    if (info->Kind() != MetadataTypeKind::Object || factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Object type factory registration is invalid");
    }
    if (behaviors.FindTypeFactory(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type factory is already registered");
    }
    const std::uint32_t factoryIdx = behaviors.typeFactories_.Size();
    behaviors.typeFactories_.PushBack({type, factory});
    static_cast<void>(behaviors.typeFactoriesIndex_.Insert(type, factoryIdx));
    return {};
}

Base::Result<void> TypeRegistry::SetContentMember(
    TypeId type,
    MemberId member) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    TypeInfo* info = MutableType(type);
    if (info == nullptr) return MissingOwnerStatus();
    if (info->contentMember_ != InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Content member is already registered");
    }
    const PropertyInfo* property = FindProperty(member);
    if (property == nullptr || property->OwnerType() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content member must be a property declared by the type");
    }
    info->contentMember_ = member;
    return {};
}

Base::Result<void> TypeRegistry::Freeze() noexcept {
    if (frozen_) return {};

    for (const TypeInfo& type : types_) {
        if (type.BaseType() != InvalidTypeId) {
            const TypeInfo* base = FindType(type.BaseType());
            if (base == nullptr) return MissingRelatedTypeStatus();
            const bool objectInheritance =
                type.Kind() == MetadataTypeKind::Object &&
                base->Kind() == MetadataTypeKind::Object;
            const bool structInheritance =
                type.Kind() == MetadataTypeKind::Struct &&
                base->Kind() == MetadataTypeKind::Struct;
            if (!objectInheritance && !structInheritance) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Metadata base types must preserve object or struct kind");
            }
        }
        if (type.Kind() == MetadataTypeKind::Enum) {
            if (type.UnderlyingType() == InvalidTypeId ||
                FindType(type.UnderlyingType()) == nullptr ||
                type.EnumValues().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Enum metadata requires an underlying type and values");
            }
        } else if (type.UnderlyingType() != InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Only enum metadata types declare an underlying type");
        }
        for (TypeId interfaceType : type.Interfaces()) {
            const TypeInfo* interfaceInfo = FindType(interfaceType);
            if (interfaceInfo == nullptr) return MissingRelatedTypeStatus();
            if (interfaceInfo->Kind() != MetadataTypeKind::Interface) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Implemented metadata type is not an interface");
            }
        }
        if (type.Kind() == MetadataTypeKind::Interface) {
            for (TypeId direct : type.Interfaces()) {
                if (direct == type.Id() ||
                    InterfaceReachable(*this, direct, type.Id(), 1U)) {
                    return InheritanceCycleStatus();
                }
            }
        }
        for (const PropertyInfo& property : type.Properties()) {
            if (FindType(property.ValueType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }

        if (type.ContentMember() != InvalidMemberId) {
            const PropertyInfo* content = FindProperty(type.ContentMember());
            if (content == nullptr || content->OwnerType() != type.Id() ||
                (static_cast<std::uint32_t>(content->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Structural)) == 0U) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Content member must be a structural property");
            }
        }
    }

    Base::Vector<std::uint8_t> state;
    state.Resize(
        types_.Size(), std::uint8_t{0U});

    Base::Vector<std::uint32_t> path;
    path.Reserve(types_.Size());
    for (std::uint32_t start = 0U; start < types_.Size(); ++start) {
        if (state[start] == 2U) continue;
        path.Clear();
        std::uint32_t current = start;
        while (state[current] != 2U) {
            if (state[current] == 1U) return InheritanceCycleStatus();
            state[current] = 1U;
            path.PushBack(current);
            const TypeId baseType = types_[current].BaseType();
            if (baseType == InvalidTypeId) break;
            const std::uint32_t* baseIndex = typeIndex_.Find(baseType);
            AERO_ASSERT(baseIndex != nullptr);
            current = *baseIndex;
        }
        for (std::uint32_t index : path) state[index] = 2U;
    }

    for (std::uint32_t selfIndex = 0U; selfIndex < types_.Size(); ++selfIndex) {
        TypeInfo& type = types_[selfIndex];
        type.ancestorChain_.Clear();
        type.ancestorIndices_.Clear();
        type.treeIn_ = 0U;
        type.treeOut_ = 0U;
        std::uint32_t currentIdx = selfIndex;
        for (std::uint32_t depth = 0U; depth <= types_.Size(); ++depth) {
            const TypeInfo& info = types_[currentIdx];
            type.ancestorChain_.PushBack(info.Id());
            type.ancestorIndices_.PushBack(currentIdx);
            if (info.BaseType() == InvalidTypeId) break;
            const std::uint32_t* baseIdx = typeIndex_.Find(info.BaseType());
            if (baseIdx == nullptr) break;
            currentIdx = *baseIdx;
        }
    }

    // Build DFS Euler tour interval [treeIn_, treeOut_] for O(1) hierarchy checks.
    const std::uint32_t n = types_.Size();
    if (n > 0U && n < 0xFFFEU) {
        Base::Vector<std::uint32_t> head;
        Base::Vector<std::uint32_t> nextSibling;
        head.Resize(n, UINT32_MAX);
        nextSibling.Resize(n, UINT32_MAX);
        if (head.Size() == n && nextSibling.Size() == n) {
            for (std::uint32_t i = 0U; i < n; ++i) {
                const TypeId baseId = types_[i].BaseType();
                if (baseId != InvalidTypeId) {
                    if (const std::uint32_t* bIdx = typeIndex_.Find(baseId)) {
                        nextSibling[i] = head[*bIdx];
                        head[*bIdx] = i;
                    }
                }
            }
            std::uint16_t timer = 0U;
            auto dfs = [&](auto& self, std::uint32_t u) noexcept -> void {
                types_[u].treeIn_ = ++timer;
                for (std::uint32_t c = head[u]; c != UINT32_MAX; c = nextSibling[c]) {
                    self(self, c);
                }
                types_[u].treeOut_ = timer;
            };
            for (std::uint32_t i = 0U; i < n; ++i) {
                if (types_[i].BaseType() == InvalidTypeId) {
                    dfs(dfs, i);
                }
            }
        }
    }

    RebuildFlatTypeIndex();
    frozen_ = true;
    return {};
}

void TypeRegistry::RebuildFlatTypeIndex() noexcept {
    flatTypeIndex_.Clear();
    const std::uint32_t count = types_.Size();
    if (count == 0U) return;
    std::uint32_t cap = 64U;
    while (cap < count * 4U) {
        cap <<= 1U;
    }
    flatTypeIndex_.Resize(cap);
    if (flatTypeIndex_.Size() < cap) {
        flatTypeIndex_.Clear();
        return;
    }
    const std::uint32_t mask = cap - 1U;
    for (std::uint32_t i = 0U; i < count; ++i) {
        const TypeId id = types_[i].Id();
        std::uint32_t slot = static_cast<std::uint32_t>(id ^ (id >> 32U)) & mask;
        while (flatTypeIndex_[slot].id != InvalidTypeId) {
            slot = (slot + 1U) & mask;
        }
        flatTypeIndex_[slot].id = id;
        flatTypeIndex_[slot].index = i;
    }
}

Base::Result<Base::HashCode> TypeRegistry::ComputeHash() const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry hash requires a frozen registry");
    }

    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.DESCRIPTORS.V2";
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(2U);

    Base::Vector<const TypeInfo*> types;
    types.Reserve(TypeCount());
    for (const TypeInfo& type : Types()) {
        types.PushBack(&type);
    }
    Base::Result<void> result = SortInfoById(types);
    if (!result) return result.GetStatus();

    builder.AddU32(types.Size());
    for (const TypeInfo* type : types) {
        builder.AddU64(type->Id());
        builder.AddU64(type->BaseType());
        builder.AddU64(type->UnderlyingType());
        builder.AddByte(static_cast<std::uint8_t>(type->Kind()));
        builder.AddU32(static_cast<std::uint32_t>(type->Flags()));
        builder.AddString(type->XamlNamespace());
        builder.AddString(type->Name());

        Base::Vector<TypeId> interfaces;
        interfaces.Append(type->Interfaces());
        for (std::uint32_t index = 1U;
             index < interfaces.Size(); ++index) {
            const TypeId value = interfaces[index];
            std::uint32_t cursor = index;
            while (cursor > 0U &&
                   value < interfaces[cursor - 1U]) {
                interfaces[cursor] = interfaces[cursor - 1U];
                --cursor;
            }
            interfaces[cursor] = value;
        }
        builder.AddU32(interfaces.Size());
        for (TypeId interfaceType : interfaces) {
            builder.AddU64(interfaceType);
        }
    }

    Base::Vector<const PropertyInfo*> properties;
    properties.Reserve(PropertyCount());
    Base::Vector<const EnumValueInfo*> enumValues;
    enumValues.Reserve(EnumValueCount());
    Base::Vector<const EventInfo*> events;
    events.Reserve(EventCount());
    for (const TypeInfo& type : Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            properties.PushBack(&property);
        }
        for (const EnumValueInfo& value : type.EnumValues()) {
            enumValues.PushBack(&value);
        }
        for (const EventInfo& eventInfo : type.Events()) {
            events.PushBack(&eventInfo);
        }
    }

    result = SortInfoById(properties);
    if (!result) return result.GetStatus();
    builder.AddU32(properties.Size());
    for (const PropertyInfo* property : properties) {
        builder.AddU64(property->Id());
        builder.AddU64(property->OwnerType());
        builder.AddU64(property->ValueType());
        builder.AddU32(static_cast<std::uint32_t>(property->Flags()));
        builder.AddString(property->Name());
    }

    result = SortInfoById(enumValues);
    if (!result) return result.GetStatus();
    builder.AddU32(enumValues.Size());
    for (const EnumValueInfo* value : enumValues) {
        builder.AddU64(value->Id());
        builder.AddU64(value->OwnerType());
        builder.AddU64(value->RawValue());
        builder.AddString(value->Name());
    }

    result = SortInfoById(events);
    if (!result) return result.GetStatus();
    builder.AddU32(events.Size());
    for (const EventInfo* eventInfo : events) {
        builder.AddU64(eventInfo->Id());
        builder.AddU64(eventInfo->OwnerType());
        builder.AddU64(eventInfo->EventArgsType());
        builder.AddU32(
            static_cast<std::uint32_t>(eventInfo->Flags()));
        builder.AddString(eventInfo->Name());
    }
    return builder.Finish();
}

const TypeInfo* TypeRegistry::FindType(TypeId id) const noexcept {
    if (id == InvalidTypeId) return nullptr;
    if (!flatTypeIndex_.Empty()) {
        const std::uint32_t mask = flatTypeIndex_.Size() - 1U;
        std::uint32_t slot = static_cast<std::uint32_t>(id ^ (id >> 32U)) & mask;
        for (;;) {
            const FlatTypeSlot& entry = flatTypeIndex_[slot];
            if (entry.id == id) return &types_[entry.index];
            if (entry.id == InvalidTypeId) return nullptr;
            slot = (slot + 1U) & mask;
        }
    }
    const std::uint32_t* index = typeIndex_.Find(id);
    return index != nullptr ? TypeAt(*index) : nullptr;
}

const TypeInfo* TypeRegistry::FindType(
    Base::StringView xamlNamespace,
    Base::StringView name) const noexcept {
    const TypeInfo* type = FindType(MakeTypeId(xamlNamespace, name));
    return type != nullptr && type->XamlNamespace() == xamlNamespace &&
        type->Name() == name ? type : nullptr;
}

const PropertyInfo* TypeRegistry::FindProperty(MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr ? PropertyAt(*location) : nullptr;
}

const PropertyInfo* TypeRegistry::FindProperty(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    if (frozen_) {
        const TypeInfo* type = FindType(ownerType);
        if (type != nullptr && !type->AncestorChain().Empty()) {
            for (TypeId current : type->AncestorChain()) {
                const PropertyInfo* property = FindProperty(
                    MakeMemberId(current, MemberKind::Property, name));
                if (property != nullptr && property->OwnerType() == current &&
                    property->Name() == name) {
                    return property;
                }
                if (!includeBaseTypes) return nullptr;
            }
            return nullptr;
        }
    }
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const PropertyInfo* property = FindProperty(
            MakeMemberId(current, MemberKind::Property, name));
        if (property != nullptr && property->OwnerType() == current &&
            property->Name() == name) return property;
        if (!includeBaseTypes) return nullptr;
        const TypeInfo* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}


const EnumValueInfo* TypeRegistry::FindEnumValue(MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr ? EnumValueAt(*location) : nullptr;
}

const EnumValueInfo* TypeRegistry::FindEnumValue(
    TypeId ownerType,
    Base::StringView name) const noexcept {
    const EnumValueInfo* value = FindEnumValue(
        MakeMemberId(ownerType, MemberKind::EnumValue, name));
    return value != nullptr && value->OwnerType() == ownerType &&
        value->Name() == name ? value : nullptr;
}

const EnumValueInfo* TypeRegistry::FindEnumValue(
    TypeId ownerType,
    std::uint64_t rawValue) const noexcept {
    const TypeInfo* type = FindType(ownerType);
    if (type == nullptr) return nullptr;
    for (const EnumValueInfo& value : type->EnumValues()) {
        if (value.RawValue() == rawValue) return &value;
    }
    return nullptr;
}

bool TypeRegistry::IsEnumValue(
    TypeId type,
    std::uint64_t rawValue) const noexcept {
    const TypeInfo* info = FindType(type);
    if (info == nullptr ||
        info->Kind() != MetadataTypeKind::Enum) {
        return false;
    }
    if (!info->IsFlagsEnum()) {
        return FindEnumValue(type, rawValue) != nullptr;
    }
    std::uint64_t allowed = 0U;
    for (const EnumValueInfo& value : info->EnumValues()) {
        allowed |= value.RawValue();
    }
    return (rawValue & ~allowed) == 0U;
}

const EventInfo* TypeRegistry::FindEvent(MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr ? EventAt(*location) : nullptr;
}

const EventInfo* TypeRegistry::FindEvent(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    if (frozen_) {
        const TypeInfo* type = FindType(ownerType);
        if (type != nullptr && !type->AncestorChain().Empty()) {
            for (TypeId current : type->AncestorChain()) {
                const EventInfo* eventInfo = FindEvent(
                    MakeMemberId(current, MemberKind::Event, name));
                if (eventInfo != nullptr && eventInfo->OwnerType() == current &&
                    eventInfo->Name() == name) {
                    return eventInfo;
                }
                if (!includeBaseTypes) return nullptr;
            }
            return nullptr;
        }
    }
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const EventInfo* eventInfo = FindEvent(
            MakeMemberId(current, MemberKind::Event, name));
        if (eventInfo != nullptr && eventInfo->OwnerType() == current &&
            eventInfo->Name() == name) return eventInfo;
        if (!includeBaseTypes) return nullptr;
        const TypeInfo* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

EventHandlerThunk TypeRegistry::FindEventHandler(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    if (frozen_) {
        const TypeInfo* owner = FindType(ownerType);
        if (owner != nullptr && !owner->AncestorIndices().Empty()) {
            for (std::uint32_t ancestorIndex : owner->AncestorIndices()) {
                const TypeInfo& type = types_[ancestorIndex];
                for (const EventHandlerDescriptor& handler : type.EventHandlers()) {
                    if (handler.name == name) {
                        return handler.thunk;
                    }
                }
                if (!includeBaseTypes) return nullptr;
            }
            return nullptr;
        }
    }
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const TypeInfo* type = FindType(current);
        if (type == nullptr) return nullptr;
        for (const EventHandlerDescriptor& handler : type->EventHandlers()) {
            if (handler.name == name) {
                return handler.thunk;
            }
        }
        if (!includeBaseTypes) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

TypeId TypeRegistry::FindTemplatePart(
    TypeId ownerType,
    Base::StringView name,
    bool includeBaseTypes) const noexcept {
    if (frozen_) {
        const TypeInfo* owner = FindType(ownerType);
        if (owner != nullptr && !owner->AncestorIndices().Empty()) {
            for (std::uint32_t ancestorIndex : owner->AncestorIndices()) {
                const TypeInfo& type = types_[ancestorIndex];
                for (const TemplatePartDescriptor& part : type.TemplateParts()) {
                    if (part.name == name) {
                        return part.partType;
                    }
                }
                if (!includeBaseTypes) return InvalidTypeId;
            }
            return InvalidTypeId;
        }
    }
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const TypeInfo* type = FindType(current);
        if (type == nullptr) return InvalidTypeId;
        for (const TemplatePartDescriptor& part : type->TemplateParts()) {
            if (part.name == name) {
                return part.partType;
            }
        }
        if (!includeBaseTypes) return InvalidTypeId;
        current = type->BaseType();
    }
    return InvalidTypeId;
}
MemberId TypeRegistry::FindContentMember(TypeId type) const noexcept {
    if (frozen_) {
        const TypeInfo* owner = FindType(type);
        if (owner != nullptr && !owner->AncestorIndices().Empty()) {
            for (std::uint32_t ancestorIndex : owner->AncestorIndices()) {
                const TypeInfo& info = types_[ancestorIndex];
                if (info.ContentMember() != InvalidMemberId) {
                    return info.ContentMember();
                }
            }
            return InvalidMemberId;
        }
    }
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const TypeInfo* info = FindType(current);
        if (info == nullptr) return InvalidMemberId;
        if (info->ContentMember() != InvalidMemberId) {
            return info->ContentMember();
        }
        current = info->BaseType();
    }
    return InvalidMemberId;
}

bool TypeRegistry::IsDerivedFrom(
    TypeId type,
    TypeId expectedBase) const noexcept {
    if (type == InvalidTypeId || expectedBase == InvalidTypeId) return false;
    if (type == expectedBase) return true;
    const TypeInfo* info = FindType(type);
    if (info == nullptr) return false;

    if (frozen_) {
        const TypeInfo* baseInfo = FindType(expectedBase);
        if (baseInfo != nullptr && info->TreeIn() != 0U && baseInfo->TreeIn() != 0U) {
            return info->IsSubtreeOf(*baseInfo);
        }
        for (TypeId ancestor : info->ancestorChain_) {
            if (ancestor == expectedBase) return true;
        }
        return false;
    }

    TypeId current = info->BaseType();
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        if (current == expectedBase) return true;
        const TypeInfo* baseInfo = FindType(current);
        if (baseInfo == nullptr) break;
        current = baseInfo->BaseType();
    }
    return false;
}

bool TypeRegistry::Implements(
    TypeId type,
    TypeId interfaceType) const noexcept {
    const TypeInfo* target = FindType(interfaceType);
    return target != nullptr &&
        target->Kind() == MetadataTypeKind::Interface &&
        TypeImplements(*this, type, interfaceType, 0U);
}

bool TypeRegistry::IsAssignableFrom(
    TypeId targetType,
    TypeId sourceType) const noexcept {
    if (targetType == InvalidTypeId || sourceType == InvalidTypeId) return false;
    if (targetType == sourceType) return true;
    const TypeInfo* target = FindType(targetType);
    if (target == nullptr) return false;
    return target->Kind() == MetadataTypeKind::Interface
        ? Implements(sourceType, targetType)
        : IsDerivedFrom(sourceType, targetType);
}

void* TypeRegistry::TryCastToInterface(
    Base::Object& object,
    TypeId interfaceType) const noexcept {
    if (interfaceType == InvalidTypeId) {
        return nullptr;
    }
    const TypeId runtimeType = object.RuntimeType();
    if (frozen_) {
        const TypeInfo* owner = FindType(runtimeType);
        if (owner == nullptr) {
            return nullptr;
        }
        for (std::uint32_t ancestorIndex : owner->AncestorIndices()) {
            const TypeInfo& info = types_[ancestorIndex];
            const Base::Span<const TypeId> interfaces = info.Interfaces();
            const Base::Span<const InterfaceCastThunk> casts =
                info.InterfaceCasts();
            for (std::uint32_t index = 0U; index < interfaces.Size(); ++index) {
                if (interfaces[index] != interfaceType) {
                    continue;
                }
                if (index >= casts.Size() || casts[index] == nullptr) {
                    return nullptr;
                }
                return casts[index](&object);
            }
        }
        return nullptr;
    }
    TypeId current = runtimeType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size();
         ++depth) {
        const TypeInfo* info = FindType(current);
        if (info == nullptr) {
            return nullptr;
        }
        const Base::Span<const TypeId> interfaces = info->Interfaces();
        const Base::Span<const InterfaceCastThunk> casts =
            info->InterfaceCasts();
        for (std::uint32_t index = 0U; index < interfaces.Size(); ++index) {
            if (interfaces[index] != interfaceType) {
                continue;
            }
            if (index >= casts.Size() || casts[index] == nullptr) {
                return nullptr;
            }
            return casts[index](&object);
        }
        current = info->BaseType();
    }
    return nullptr;
}

TypeInfo* TypeRegistry::MutableType(TypeId id) noexcept {
    std::uint32_t* index = typeIndex_.Find(id);
    return index != nullptr ? &types_[*index] : nullptr;
}

const TypeInfo* TypeRegistry::TypeAt(std::uint32_t index) const noexcept {
    return index < types_.Size() ? &types_[index] : nullptr;
}

const PropertyInfo* TypeRegistry::PropertyAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::Property) return nullptr;
    const TypeInfo* owner = TypeAt(location.typeIndex);
    return owner != nullptr && location.memberIndex < owner->properties_.Size()
        ? &owner->properties_[location.memberIndex] : nullptr;
}


const EnumValueInfo* TypeRegistry::EnumValueAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::EnumValue) return nullptr;
    const TypeInfo* owner = TypeAt(location.typeIndex);
    return owner != nullptr && location.memberIndex < owner->enumValues_.Size()
        ? &owner->enumValues_[location.memberIndex] : nullptr;
}

const EventInfo* TypeRegistry::EventAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::Event) return nullptr;
    const TypeInfo* owner = TypeAt(location.typeIndex);
    return owner != nullptr && location.memberIndex < owner->events_.Size()
        ? &owner->events_[location.memberIndex] : nullptr;
}


} // namespace Aero::Meta


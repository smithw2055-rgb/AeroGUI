#include <Aero/Meta/TypeRegistry.hpp>
#include "BehaviorTable.hpp"

#include <Aero/Base/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace Aero::Core {
namespace {

constexpr Base::Status EmptyTypeNameStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Type namespace and name must be non-empty UTF-8 strings");
}

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
    const TypeInfo* info = registry.FindType(current);
    if (info == nullptr) return false;
    for (TypeId direct : info->Interfaces()) {
        if (direct == target ||
            InterfaceReachable(registry, direct, target, depth + 1U)) {
            return true;
        }
    }
    return TypeImplements(
        registry, info->BaseType(), target, depth + 1U);
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
    Base::Detail::StableMetadataIdBuilder builder;
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

Base::Result<TypeId> TypeRegistry::TryRegisterType(
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
        Base::Result<void> reserved = behaviors.typeFactories_.TryReserve(
            behaviors.typeFactories_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    TypeInfo info;
    info.id_ = id;
    info.baseType_ = registration.baseType;
    info.underlyingType_ = registration.underlyingType;
    info.kind_ = kind;
    info.flags_ = flags;
    Base::Result<void> result =
        info.xamlNamespace_.TryAssign(registration.xamlNamespace);
    if (!result) return result.GetStatus();
    result = info.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();
    result = info.interfaces_.TryReserve(registration.interfaces.Size());
    if (!result) return result.GetStatus();
    for (TypeId interfaceType : registration.interfaces) {
        if (interfaceType == InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Implemented interface type must be valid");
        }
        for (TypeId existing : info.interfaces_) {
            if (existing == interfaceType) return DuplicateMemberStatus();
        }
        result = info.interfaces_.TryPushBack(interfaceType);
        if (!result) return result.GetStatus();
    }

    const std::uint32_t index = types_.Size();
    result = types_.TryPushBack(std::move(info));
    if (!result) return result.GetStatus();

    bool factoryAdded = false;
    if (registration.factory != nullptr) {
        result = behaviors.typeFactories_.TryPushBack({id, registration.factory});
        if (!result) {
            types_.PopBack();
            return result.GetStatus();
        }
        factoryAdded = true;
    }

    Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
        typeIndex_.TryInsert(id, index);
    if (!inserted || !inserted.Value().inserted) {
        if (factoryAdded) behaviors.typeFactories_.PopBack();
        types_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::TryRegisterInterface(
    TypeId ownerType,
    TypeId interfaceType) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (interfaceType == InvalidTypeId || ownerType == interfaceType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implemented interface relation is invalid");
    }
    TypeInfo* owner = MutableType(ownerType);
    if (owner == nullptr) return MissingOwnerStatus();
    for (TypeId existing : owner->interfaces_) {
        if (existing == interfaceType) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Implemented interface is already registered");
        }
    }
    return owner->interfaces_.TryPushBack(interfaceType);
}

Base::Result<MemberId> TypeRegistry::TryRegisterProperty(
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
        Base::Result<void> reserved = behaviors.propertyAccessors_.TryReserve(
            behaviors.propertyAccessors_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    TypeInfo& owner = types_[*ownerIndex];
    PropertyInfo property;
    property.id_ = id;
    property.ownerType_ = ownerType;
    property.valueType_ = registration.valueType;
    property.flags_ = registration.flags;
    Base::Result<void> result = property.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t propertyIndex = owner.properties_.Size();
    result = owner.properties_.TryPushBack(std::move(property));
    if (!result) return result.GetStatus();

    bool behaviorAdded = false;
    if (hasBehavior) {
        result = behaviors.propertyAccessors_.TryPushBack({
            id, registration.access, registration.get, registration.set,
            registration.provider, registration.context});
        if (!result) {
            owner.properties_.PopBack();
            return result.GetStatus();
        }
        behaviorAdded = true;
    }

    const MemberLocation location{
        *ownerIndex, propertyIndex, MemberKind::Property};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        if (behaviorAdded) behaviors.propertyAccessors_.PopBack();
        owner.properties_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterField(
    BehaviorTable& behaviors,
    TypeId ownerType,
    const FieldRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.name.Empty()) return EmptyMemberNameStatus();
    if (registration.valueType == InvalidTypeId || registration.get == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value field requires a valid type and getter");
    }
    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();
    TypeInfo& owner = types_[*ownerIndex];
    if (owner.Kind() != MetadataTypeKind::Struct) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value fields may only be declared by struct metadata types");
    }

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::Field, registration.name);
    if (memberIndex_.Find(id) != nullptr) return DuplicateMemberStatus();

    Base::Result<void> result = behaviors.valueMemberAccessors_.TryReserve(
        behaviors.valueMemberAccessors_.Size() + 1U);
    if (!result) return result.GetStatus();

    FieldInfo field;
    field.id_ = id;
    field.ownerType_ = ownerType;
    field.valueType_ = registration.valueType;
    field.flags_ = registration.flags;
    result = field.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t fieldIndex = owner.fields_.Size();
    result = owner.fields_.TryPushBack(std::move(field));
    if (!result) return result.GetStatus();
    result = behaviors.valueMemberAccessors_.TryPushBack({
        id, registration.get, registration.set, registration.context});
    if (!result) {
        owner.fields_.PopBack();
        return result.GetStatus();
    }

    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(
            id, {*ownerIndex, fieldIndex, MemberKind::Field});
    if (!inserted || !inserted.Value().inserted) {
        behaviors.valueMemberAccessors_.PopBack();
        owner.fields_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterEnumValue(
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
    Base::Result<void> result = value.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();
    const std::uint32_t valueIndex = owner.enumValues_.Size();
    result = owner.enumValues_.TryPushBack(std::move(value));
    if (!result) return result.GetStatus();

    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(
            id, {*ownerIndex, valueIndex, MemberKind::EnumValue});
    if (!inserted || !inserted.Value().inserted) {
        owner.enumValues_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterEvent(
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
    Base::Result<void> result = eventInfo.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t eventIndex = owner.events_.Size();
    result = owner.events_.TryPushBack(std::move(eventInfo));
    if (!result) return result.GetStatus();

    const MemberLocation location{
        *ownerIndex, eventIndex, MemberKind::Event};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        owner.events_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterMethod(
    BehaviorTable& behaviors,
    TypeId ownerType,
    const MethodRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.name.Empty() || registration.invoke == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Method name and invoke callback are required");
    }
    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) return MissingOwnerStatus();

    Base::Vector<TypeId> signature;
    Base::Result<void> result = signature.TryReserve(
        registration.parameters.Size());
    if (!result) return result.GetStatus();
    for (const MethodParameterRegistration& parameter :
         registration.parameters) {
        if (parameter.name.Empty() || parameter.type == InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Method parameters require a name and valid type");
        }
        result = signature.TryPushBack(parameter.type);
        if (!result) return result.GetStatus();
    }

    const MemberId id = MakeMethodId(ownerType, registration.name,
        {signature.Data(), signature.Size()});
    if (memberIndex_.Find(id) != nullptr) return DuplicateMemberStatus();

    result = behaviors.methodInvokers_.TryReserve(
        behaviors.methodInvokers_.Size() + 1U);
    if (!result) return result.GetStatus();

    TypeInfo& owner = types_[*ownerIndex];
    MethodInfo method;
    method.id_ = id;
    method.ownerType_ = ownerType;
    method.returnType_ = registration.returnType;
    method.flags_ = registration.flags;
    result = method.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();
    result = method.parameters_.TryReserve(registration.parameters.Size());
    if (!result) return result.GetStatus();
    for (const MethodParameterRegistration& source :
         registration.parameters) {
        MethodParameterInfo parameter;
        parameter.type_ = source.type;
        result = parameter.name_.TryAssign(source.name);
        if (!result) return result.GetStatus();
        result = method.parameters_.TryPushBack(std::move(parameter));
        if (!result) return result.GetStatus();
    }

    const std::uint32_t methodIndex = owner.methods_.Size();
    result = owner.methods_.TryPushBack(std::move(method));
    if (!result) return result.GetStatus();

    result = behaviors.methodInvokers_.TryPushBack(
        {id, registration.invoke, registration.context});
    if (!result) {
        owner.methods_.PopBack();
        return result.GetStatus();
    }

    const MemberLocation location{
        *ownerIndex, methodIndex, MemberKind::Method};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        behaviors.methodInvokers_.PopBack();
        owner.methods_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::TrySetFactory(
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
    return behaviors.typeFactories_.TryPushBack({type, factory});
}

Base::Result<void> TypeRegistry::TrySetContentMember(
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
        for (const FieldInfo& field : type.Fields()) {
            if (FindType(field.ValueType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }
        for (const EventInfo& eventInfo : type.Events()) {
            if (FindType(eventInfo.EventArgsType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }
        for (const MethodInfo& method : type.Methods()) {
            if (method.ReturnType() != InvalidTypeId &&
                FindType(method.ReturnType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
            for (const MethodParameterInfo& parameter : method.Parameters()) {
                if (FindType(parameter.Type()) == nullptr) {
                    return MissingRelatedTypeStatus();
                }
            }
        }
        if (type.ContentMember() != InvalidMemberId) {
            const PropertyInfo* content = FindProperty(type.ContentMember());
            if (content == nullptr || content->OwnerType() != type.Id() ||
                (static_cast<std::uint32_t>(content->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Structural)) == 0U) {
                std::fprintf(
                    stderr,
                    "Aero metadata invalid content member: type=%.*s member=%u\n",
                    static_cast<int>(
                        type.Name().SizeBytes()),
                    type.Name().Data(),
                    static_cast<unsigned>(
                        type.ContentMember()));
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Content member must be a structural property");
            }
        }
    }

    Base::Vector<std::uint8_t> state;
    Base::Result<void> result = state.TryResize(
        types_.Size(), std::uint8_t{0U});
    if (!result) return result.GetStatus();

    Base::Vector<std::uint32_t> path;
    result = path.TryReserve(types_.Size());
    if (!result) return result.GetStatus();

    for (std::uint32_t start = 0U; start < types_.Size(); ++start) {
        if (state[start] == 2U) continue;
        path.Clear();
        std::uint32_t current = start;
        while (state[current] != 2U) {
            if (state[current] == 1U) return InheritanceCycleStatus();
            state[current] = 1U;
            result = path.TryPushBack(current);
            if (!result) return result.GetStatus();
            const TypeId baseType = types_[current].BaseType();
            if (baseType == InvalidTypeId) break;
            const std::uint32_t* baseIndex = typeIndex_.Find(baseType);
            AERO_ASSERT(baseIndex != nullptr);
            current = *baseIndex;
        }
        for (std::uint32_t index : path) state[index] = 2U;
    }

    frozen_ = true;
    return {};
}

Base::Result<Base::HashCode> TypeRegistry::ComputeHash() const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry hash requires a frozen registry");
    }

    Base::Detail::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.DESCRIPTORS.V2";
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(2U);

    Base::Vector<const TypeInfo*> types;
    Base::Result<void> result = types.TryReserve(TypeCount());
    if (!result) return result.GetStatus();
    for (const TypeInfo& type : Types()) {
        result = types.TryPushBack(&type);
        if (!result) return result.GetStatus();
    }
    result = SortInfoById(types);
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
        result = interfaces.TryAppend(type->Interfaces());
        if (!result) return result.GetStatus();
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
    result = properties.TryReserve(PropertyCount());
    if (!result) return result.GetStatus();
    Base::Vector<const FieldInfo*> fields;
    result = fields.TryReserve(FieldCount());
    if (!result) return result.GetStatus();
    Base::Vector<const EnumValueInfo*> enumValues;
    result = enumValues.TryReserve(EnumValueCount());
    if (!result) return result.GetStatus();
    Base::Vector<const EventInfo*> events;
    result = events.TryReserve(EventCount());
    if (!result) return result.GetStatus();
    Base::Vector<const MethodInfo*> methods;
    result = methods.TryReserve(MethodCount());
    if (!result) return result.GetStatus();

    for (const TypeInfo& type : Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            result = properties.TryPushBack(&property);
            if (!result) return result.GetStatus();
        }
        for (const FieldInfo& field : type.Fields()) {
            result = fields.TryPushBack(&field);
            if (!result) return result.GetStatus();
        }
        for (const EnumValueInfo& value : type.EnumValues()) {
            result = enumValues.TryPushBack(&value);
            if (!result) return result.GetStatus();
        }
        for (const EventInfo& eventInfo : type.Events()) {
            result = events.TryPushBack(&eventInfo);
            if (!result) return result.GetStatus();
        }
        for (const MethodInfo& method : type.Methods()) {
            result = methods.TryPushBack(&method);
            if (!result) return result.GetStatus();
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

    result = SortInfoById(fields);
    if (!result) return result.GetStatus();
    builder.AddU32(fields.Size());
    for (const FieldInfo* field : fields) {
        builder.AddU64(field->Id());
        builder.AddU64(field->OwnerType());
        builder.AddU64(field->ValueType());
        builder.AddU32(static_cast<std::uint32_t>(field->Flags()));
        builder.AddString(field->Name());
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

    result = SortInfoById(methods);
    if (!result) return result.GetStatus();
    builder.AddU32(methods.Size());
    for (const MethodInfo* method : methods) {
        builder.AddU64(method->Id());
        builder.AddU64(method->OwnerType());
        builder.AddU64(method->ReturnType());
        builder.AddU32(static_cast<std::uint32_t>(method->Flags()));
        builder.AddString(method->Name());
        builder.AddU32(method->Parameters().Size());
        for (const MethodParameterInfo& parameter :
             method->Parameters()) {
            builder.AddU64(parameter.Type());
            builder.AddString(parameter.Name());
        }
    }
    return builder.Finish();
}

const TypeInfo* TypeRegistry::FindType(TypeId id) const noexcept {
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

const FieldInfo* TypeRegistry::FindField(MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr ? FieldAt(*location) : nullptr;
}

const FieldInfo* TypeRegistry::FindField(
    TypeId ownerType,
    Base::StringView name) const noexcept {
    const FieldInfo* field = FindField(
        MakeMemberId(ownerType, MemberKind::Field, name));
    return field != nullptr && field->OwnerType() == ownerType &&
        field->Name() == name ? field : nullptr;
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

const MethodInfo* TypeRegistry::FindMethod(MemberId id) const noexcept {
    const MemberLocation* location = memberIndex_.Find(id);
    return location != nullptr ? MethodAt(*location) : nullptr;
}

const MethodInfo* TypeRegistry::FindMethod(
    TypeId ownerType,
    Base::StringView name,
    Base::Span<const TypeId> parameterTypes,
    bool includeBaseTypes) const noexcept {
    TypeId current = ownerType;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        const MethodInfo* method = FindMethod(
            MakeMethodId(current, name, parameterTypes));
        if (method != nullptr && method->OwnerType() == current &&
            method->Name() == name) return method;
        if (!includeBaseTypes) return nullptr;
        const TypeInfo* type = FindType(current);
        if (type == nullptr) return nullptr;
        current = type->BaseType();
    }
    return nullptr;
}

MemberId TypeRegistry::FindContentMember(TypeId type) const noexcept {
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
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size(); ++depth) {
        if (current == expectedBase) return true;
        const TypeInfo* info = FindType(current);
        if (info == nullptr) return false;
        current = info->BaseType();
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

const FieldInfo* TypeRegistry::FieldAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::Field) return nullptr;
    const TypeInfo* owner = TypeAt(location.typeIndex);
    return owner != nullptr && location.memberIndex < owner->fields_.Size()
        ? &owner->fields_[location.memberIndex] : nullptr;
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

const MethodInfo* TypeRegistry::MethodAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::Method) return nullptr;
    const TypeInfo* owner = TypeAt(location.typeIndex);
    return owner != nullptr && location.memberIndex < owner->methods_.Size()
        ? &owner->methods_[location.memberIndex] : nullptr;
}

} // namespace Aero::Core

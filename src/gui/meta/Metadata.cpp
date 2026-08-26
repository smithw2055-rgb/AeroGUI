// Consolidated implementation. Keep sections ordered by dependency.

// ===== TypeRegistry =====

#include <Aero/Value.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

#include <Aero/Base/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace Aero::Meta {
namespace {

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

    Base::Result<Base::HashMap<TypeId, RuntimeTypeBinding>::InsertResult>
        inserted = bindings.Insert(token, std::move(binding));
    if (!inserted) return inserted.GetStatus();
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
        Base::Result<void> reserved = behaviors.typeFactories_.Reserve(
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
        info.xamlNamespace_.Assign(registration.xamlNamespace);
    if (!result) return result.GetStatus();
    result = info.name_.Assign(registration.name);
    if (!result) return result.GetStatus();
    result = info.interfaces_.Reserve(registration.interfaces.Size());
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
        result = info.interfaces_.PushBack(interfaceType);
        if (!result) return result.GetStatus();
        result = info.interfaceCasts_.PushBack(nullptr);
        if (!result) return result.GetStatus();
    }

    const std::uint32_t index = types_.Size();
    result = types_.PushBack(std::move(info));
    if (!result) return result.GetStatus();

    bool factoryAdded = false;
    if (registration.factory != nullptr) {
        result = behaviors.typeFactories_.PushBack({id, registration.factory});
        if (!result) {
            types_.PopBack();
            return result.GetStatus();
        }
        factoryAdded = true;
    }

    Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
        typeIndex_.Insert(id, index);
    if (!inserted || !inserted.Value().inserted) {
        if (factoryAdded) behaviors.typeFactories_.PopBack();
        types_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
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
    for (TypeId existing : owner->interfaces_) {
        if (existing == interfaceType) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Implemented interface is already registered");
        }
    }
    while (owner->interfaceCasts_.Size() < owner->interfaces_.Size()) {
        Base::Result<void> padded = owner->interfaceCasts_.PushBack(nullptr);
        if (!padded) return padded.GetStatus();
    }
    Base::Result<void> stored = owner->interfaces_.PushBack(interfaceType);
    if (!stored) return stored.GetStatus();
    return owner->interfaceCasts_.PushBack(cast);
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
        Base::Result<void> reserved = behaviors.propertyAccessors_.Reserve(
            behaviors.propertyAccessors_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
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
    result = owner.properties_.PushBack(std::move(property));
    if (!result) return result.GetStatus();

    bool behaviorAdded = false;
    if (hasBehavior) {
        result = behaviors.propertyAccessors_.PushBack({
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
        memberIndex_.Insert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        if (behaviorAdded) behaviors.propertyAccessors_.PopBack();
        owner.properties_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::RegisterField(
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

    Base::Result<void> result = behaviors.valueMemberAccessors_.Reserve(
        behaviors.valueMemberAccessors_.Size() + 1U);
    if (!result) return result.GetStatus();

    FieldInfo field;
    field.id_ = id;
    field.ownerType_ = ownerType;
    field.valueType_ = registration.valueType;
    field.flags_ = registration.flags;
    result = field.name_.Assign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t fieldIndex = owner.fields_.Size();
    result = owner.fields_.PushBack(std::move(field));
    if (!result) return result.GetStatus();
    result = behaviors.valueMemberAccessors_.PushBack({
        id, registration.get, registration.set, registration.context});
    if (!result) {
        owner.fields_.PopBack();
        return result.GetStatus();
    }

    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.Insert(
            id, {*ownerIndex, fieldIndex, MemberKind::Field});
    if (!inserted || !inserted.Value().inserted) {
        behaviors.valueMemberAccessors_.PopBack();
        owner.fields_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
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
    result = owner.enumValues_.PushBack(std::move(value));
    if (!result) return result.GetStatus();

    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.Insert(
            id, {*ownerIndex, valueIndex, MemberKind::EnumValue});
    if (!inserted || !inserted.Value().inserted) {
        owner.enumValues_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
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
    result = owner.events_.PushBack(std::move(eventInfo));
    if (!result) return result.GetStatus();

    const MemberLocation location{
        *ownerIndex, eventIndex, MemberKind::Event};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.Insert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        owner.events_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::RegisterMethod(
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
    Base::Result<void> result = signature.Reserve(
        registration.parameters.Size());
    if (!result) return result.GetStatus();
    for (const MethodParameterRegistration& parameter :
         registration.parameters) {
        if (parameter.name.Empty() || parameter.type == InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Method parameters require a name and valid type");
        }
        result = signature.PushBack(parameter.type);
        if (!result) return result.GetStatus();
    }

    const MemberId id = MakeMethodId(ownerType, registration.name,
        {signature.Data(), signature.Size()});
    if (memberIndex_.Find(id) != nullptr) return DuplicateMemberStatus();

    result = behaviors.methodInvokers_.Reserve(
        behaviors.methodInvokers_.Size() + 1U);
    if (!result) return result.GetStatus();

    TypeInfo& owner = types_[*ownerIndex];
    MethodInfo method;
    method.id_ = id;
    method.ownerType_ = ownerType;
    method.returnType_ = registration.returnType;
    method.flags_ = registration.flags;
    result = method.name_.Assign(registration.name);
    if (!result) return result.GetStatus();
    result = method.parameters_.Reserve(registration.parameters.Size());
    if (!result) return result.GetStatus();
    for (const MethodParameterRegistration& source :
         registration.parameters) {
        MethodParameterInfo parameter;
        parameter.type_ = source.type;
        result = parameter.name_.Assign(source.name);
        if (!result) return result.GetStatus();
        result = method.parameters_.PushBack(std::move(parameter));
        if (!result) return result.GetStatus();
    }

    const std::uint32_t methodIndex = owner.methods_.Size();
    result = owner.methods_.PushBack(std::move(method));
    if (!result) return result.GetStatus();

    result = behaviors.methodInvokers_.PushBack(
        {id, registration.invoke, registration.context});
    if (!result) {
        owner.methods_.PopBack();
        return result.GetStatus();
    }

    const MemberLocation location{
        *ownerIndex, methodIndex, MemberKind::Method};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.Insert(id, location);
    if (!inserted || !inserted.Value().inserted) {
        behaviors.methodInvokers_.PopBack();
        owner.methods_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
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
    return behaviors.typeFactories_.PushBack({type, factory});
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
    Base::Result<void> result = state.Resize(
        types_.Size(), std::uint8_t{0U});
    if (!result) return result.GetStatus();

    Base::Vector<std::uint32_t> path;
    result = path.Reserve(types_.Size());
    if (!result) return result.GetStatus();

    for (std::uint32_t start = 0U; start < types_.Size(); ++start) {
        if (state[start] == 2U) continue;
        path.Clear();
        std::uint32_t current = start;
        while (state[current] != 2U) {
            if (state[current] == 1U) return InheritanceCycleStatus();
            state[current] = 1U;
            result = path.PushBack(current);
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

    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.DESCRIPTORS.V2";
    builder.AddText(
        domain,
        static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(2U);

    Base::Vector<const TypeInfo*> types;
    Base::Result<void> result = types.Reserve(TypeCount());
    if (!result) return result.GetStatus();
    for (const TypeInfo& type : Types()) {
        result = types.PushBack(&type);
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
        result = interfaces.Append(type->Interfaces());
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
    result = properties.Reserve(PropertyCount());
    if (!result) return result.GetStatus();
    Base::Vector<const FieldInfo*> fields;
    result = fields.Reserve(FieldCount());
    if (!result) return result.GetStatus();
    Base::Vector<const EnumValueInfo*> enumValues;
    result = enumValues.Reserve(EnumValueCount());
    if (!result) return result.GetStatus();
    Base::Vector<const EventInfo*> events;
    result = events.Reserve(EventCount());
    if (!result) return result.GetStatus();
    Base::Vector<const MethodInfo*> methods;
    result = methods.Reserve(MethodCount());
    if (!result) return result.GetStatus();

    for (const TypeInfo& type : Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            result = properties.PushBack(&property);
            if (!result) return result.GetStatus();
        }
        for (const FieldInfo& field : type.Fields()) {
            result = fields.PushBack(&field);
            if (!result) return result.GetStatus();
        }
        for (const EnumValueInfo& value : type.EnumValues()) {
            result = enumValues.PushBack(&value);
            if (!result) return result.GetStatus();
        }
        for (const EventInfo& eventInfo : type.Events()) {
            result = events.PushBack(&eventInfo);
            if (!result) return result.GetStatus();
        }
        for (const MethodInfo& method : type.Methods()) {
            result = methods.PushBack(&method);
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

void* TypeRegistry::TryCastToInterface(
    Base::Object& object,
    TypeId interfaceType) const noexcept {
    if (interfaceType == InvalidTypeId) {
        return nullptr;
    }
    TypeId current = object.RuntimeType();
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

} // namespace Aero::Meta


// ===== MetadataAuthoring =====

#include <Aero/Meta.hpp>


namespace Aero::Meta {

MetadataAuthoringSession::MetadataAuthoringSession(
    Meta::Registration& context,
    const TypeRegistration& registration,
    TypeId expectedType) noexcept
    : context_(&context) {
    Base::Result<TypeId> result =
        context_->Types().RegisterType(registration);
    if (!result) {
        status_ = result.GetStatus();
        return;
    }
    type_ = result.Value();
    if (type_ != expectedType) {
        status_ = Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed metadata descriptor does not match TypeOf<T>()");
    }
}

MetadataAuthoringSession&
MetadataAuthoringSession::Implements(
    TypeId interfaceType,
    InterfaceCastThunk cast) noexcept {
    if (Ok()) {
        Record(context_->Types().RegisterInterface(
            type_, interfaceType, cast));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Factory(
    ObjectFactory factory) noexcept {
    if (Ok()) {
        Record(context_->Types().SetFactory(
            type_, factory));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::PropertyChangeNotifications(
    PropertyChangeSubscribeCallback subscribe,
    PropertyChangeUnsubscribeCallback unsubscribe,
    void* callbackContext) noexcept {
    if (Ok()) {
        Record(context_->Types().
            RegisterPropertyChangeNotification({
                type_,
                subscribe,
                unsubscribe,
                callbackContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::CollectionChangeNotifications(
    CollectionChangeSubscribeCallback subscribe,
    CollectionChangeUnsubscribeCallback unsubscribe,
    void* callbackContext) noexcept {
    if (Ok()) {
        Record(context_->Types().
            RegisterCollectionChangeNotification({
                type_,
                subscribe,
                unsubscribe,
                callbackContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::DependencyProperty(
    DependencyPropertyHandle declaredHandle,
    Base::StringView name,
    TypeId valueType,
    Value defaultValue,
    PropertyMetadataFlags metadataFlags,
    DependencyPropertyFlags propertyFlags,
    ValidateValueCallback validate,
    CoerceValueCallback coerce,
    PropertyChangedCallback changed,
    UpdateSourceTrigger updateSourceTrigger) noexcept {
    if (!Ok()) return *this;
    if (name.Empty() ||
        declaredHandle !=
            MakeDependencyPropertyHandle(type_, name)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed dependency property handle does not match owner and name"));
    }

    DependencyPropertyRegistration registration;
    registration.name = name;
    registration.ownerType = type_;
    registration.valueType = valueType;
    registration.flags = propertyFlags;
    registration.metadata.defaultValue =
        std::move(defaultValue);
    registration.metadata.flags = metadataFlags;
    registration.metadata.validate = validate;
    registration.metadata.coerce = coerce;
    registration.metadata.changed = changed;
    registration.metadata.defaultUpdateSourceTrigger =
        updateSourceTrigger;

    Base::Result<DependencyPropertyRegistrationResult>
        registered =
            context_->DependencyProperties().Register(
                registration);
    if (!registered) {
        return Fail(registered.GetStatus());
    }
    if (registered.Value().property != declaredHandle) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Dependency property registry returned a different handle"));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Override(
    DependencyPropertyHandle property,
    TypeId ownerType,
    PropertyMetadata metadata) noexcept {
    if (Ok()) {
        Record(context_->DependencyProperties().
            OverrideMetadata(
                property,
                ownerType,
                std::move(metadata)));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::AddOwner(
    DependencyPropertyHandle property,
    TypeId ownerType,
    PropertyMetadata metadata) noexcept {
    if (Ok()) {
        Record(context_->DependencyProperties().
            AddOwner(
                property,
                ownerType,
                metadata));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::RoutedEvent(
    RoutedEventHandle declaredHandle,
    Base::StringView name,
    TypeId eventArgsType,
    RoutingStrategy strategy) noexcept {
    if (!Ok()) return *this;
    auto* state =
        static_cast<Aero::RegistrationState*>(
            context_->state_);
    RoutedEventTable* events =
        state != nullptr ? state->events : nullptr;
    if (events == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Routed event metadata requires a registry"));
    }
    if (name.Empty() ||
        declaredHandle !=
            MakeRoutedEventHandle(type_, name)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Typed routed event handle does not match owner and name"));
    }
    Base::Result<RoutedEventHandle> registered =
        events->Register({
            name,
            type_,
            eventArgsType,
            strategy});
    if (!registered) {
        return Fail(registered.GetStatus());
    }
    if (registered.Value() != declaredHandle) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Routed event registry returned a different handle"));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Content(
    Base::StringView name,
    TypeId valueType,
    ContentKind kind,
    ContentWriteCallback write,
    ContentClearCallback clear,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (valueType == InvalidTypeId) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content property value type is invalid"));
    }
    if ((write == nullptr) != (clear == nullptr) ||
        (write == nullptr &&
         contentFlags != ContentFlags::None)) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content access requires matching write and clear callbacks"));
    }

    PropertyFlags flags = PropertyFlags::Structural;
    if (kind == ContentKind::Collection) {
        flags = flags | PropertyFlags::Collection;
    }
    Base::Result<MemberId> member =
        context_->Types().RegisterProperty(
            type_, {name, valueType, flags});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().SetContentMember(
        type_, member.Value()));
    if (Ok() && write != nullptr) {
        Record(context_->Types().SetContentAccessor({
            type_,
            member.Value(),
            kind,
            contentFlags,
            write,
            clear,
            contentContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Collection(
    Base::StringView name,
    TypeId valueType,
    ContentWriteCallback write,
    ContentClearCallback clear,
    PropertyFlags propertyFlags,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (valueType == InvalidTypeId ||
        write == nullptr ||
        clear == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Collection property requires a value type and access callbacks"));
    }
    Base::Result<MemberId> member =
        context_->Types().RegisterProperty(
            type_,
            {
                name,
                valueType,
                propertyFlags |
                    PropertyFlags::Structural |
                    PropertyFlags::Collection});
    if (!member) return Fail(member.GetStatus());

    Record(context_->Types().SetContentAccessor({
        type_,
        member.Value(),
        ContentKind::Collection,
        contentFlags,
        write,
        clear,
        contentContext}));
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Property(
    const PropertyRegistration& registration) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().RegisterProperty(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Field(
    const FieldRegistration& registration) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().RegisterField(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Method(
    const MethodRegistration& registration) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().RegisterMethod(
                type_, registration);
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::EnumValueRaw(
    Base::StringView name,
    std::uint64_t rawValue) noexcept {
    if (Ok()) {
        Base::Result<MemberId> result =
            context_->Types().RegisterEnumValue(
                type_, {name, rawValue});
        Record(result);
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Content(
    MemberId member) noexcept {
    if (Ok()) {
        Record(context_->Types().SetContentMember(
            type_, member));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::ContentAccessor(
    MemberId member,
    ContentKind kind,
    ContentWriteCallback write,
    ContentClearCallback clear,
    ContentFlags contentFlags,
    void* contentContext) noexcept {
    if (!Ok()) return *this;
    if (member == InvalidMemberId ||
        write == nullptr ||
        clear == nullptr) {
        return Fail(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content accessor requires a member and matching callbacks"));
    }
    Record(context_->Types().SetContentMember(
        type_, member));
    if (Ok()) {
        Record(context_->Types().SetContentAccessor({
            type_,
            member,
            kind,
            contentFlags,
            write,
            clear,
            contentContext}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::ValueSemantics(
    const ValueTypeRegistration& registration) noexcept {
    if (Ok()) {
        Record(context_->Values().
            RegisterValueSemantics(
                type_, registration));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::TextConverter(
    TextValueConverterCallback converter) noexcept {
    if (Ok()) {
        Record(context_->Values().
            RegisterTextConverter({
                type_,
                converter,
                &context_->ValueRegistrations()}));
    }
    return *this;
}

MetadataAuthoringSession&
MetadataAuthoringSession::Fail(
    Base::Status status) noexcept {
    if (status_.IsOk() && !status.IsOk()) {
        status_ = status;
    }
    return *this;
}

Base::Result<void*>
MetadataAuthoringSession::OwnBehaviorContextRaw(
    std::size_t size,
    std::size_t alignment,
    void* source,
    void (*construct)(void*, void*) noexcept,
    void (*destroyValue)(void*) noexcept) noexcept {
    return context_->Types().Behaviors().
        OwnContextRaw(
            size,
            alignment,
            source,
            construct,
            destroyValue);
}

void MetadataAuthoringSession::ReleaseBehaviorContext(
    void* value) noexcept {
    context_->Types().Behaviors().
        ReleaseLastContext(value);
}

Base::Result<void>
MetadataAuthoringSession::Finish() const noexcept {
    return status_.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(status_);
}

void MetadataAuthoringSession::Record(
    Base::Result<void> result) noexcept {
    if (status_.IsOk() && !result) {
        status_ = result.GetStatus();
    }
}

} // namespace Aero::Meta


// ===== MetaTable =====


// Executable metadata behavior is private to Meta::Registry.

#include <Aero/DependencyProperty.hpp>


namespace Aero {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> result = order.Reserve(count);
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        result = order.PushBack(index);
        if (!result) return result.GetStatus();
    }
    for (std::uint32_t index = 1U; index < count; ++index) {
        const std::uint32_t value = order[index];
        std::uint32_t cursor = index;
        while (cursor > 0U && less(value, order[cursor - 1U])) {
            order[cursor] = order[cursor - 1U];
            --cursor;
        }
        order[cursor] = value;
    }
    return {};
}

bool HasEventFlag(EventFlags value, EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

std::uint16_t MetaTable::FacetCountBefore(
    MetadataFacetMask mask,
    MetadataFacetKind kind) noexcept {
    return CompactFacetIndex::CountBefore(mask, kind);
}

Base::Result<void> MetaTable::SetTypeFacet(
    TypeId type,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (type == InvalidTypeId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type facet reference is invalid");
    }
    FacetDraft* draft = nullptr;
    for (FacetDraft& entry : typeDrafts_) {
        if (entry.key == type) {
            draft = &entry;
            break;
        }
    }
    if (draft == nullptr) {
        Base::Result<FacetDraft*> added = typeDrafts_.EmplaceBack();
        if (!added) return added.GetStatus();
        added.Value()->key = type;
        draft = added.Value();
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::SetMemberFacet(
    MemberId member,
    MetadataFacetKind kind,
    std::uint32_t index) noexcept {
    if (member == InvalidMemberId || index == InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Member facet reference is invalid");
    }
    FacetDraft* draft = nullptr;
    for (FacetDraft& entry : memberDrafts_) {
        if (entry.key == member) {
            draft = &entry;
            break;
        }
    }
    if (draft == nullptr) {
        Base::Result<FacetDraft*> added = memberDrafts_.EmplaceBack();
        if (!added) return added.GetStatus();
        added.Value()->key = member;
        draft = added.Value();
    }
    const std::uint8_t slot = static_cast<std::uint8_t>(kind);
    if (draft->facets[slot] != InvalidFacetIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Member facet is already registered");
    }
    draft->facets[slot] = index;
    return {};
}

Base::Result<void> MetaTable::Build(
    const TypeRegistry& types,
    const BehaviorTable& behaviors,
    const DependencyPropertyRegistry& dependencyProperties,
    const RoutedEventTable& routedEvents) noexcept {
    if (sealed_) return InvalidState("MetaTable is already sealed");
    if (!types.IsFrozen() || !behaviors.IsFrozen() ||
        !dependencyProperties.IsFrozen() || !routedEvents.IsFrozen()) {
        return InvalidState(
            "Metadata sources must be sealed before building facets");
    }
    types_ = &types;

    for (const TypeInfo& type : types.Types()) {
        Base::Result<void> result;
        const TypeFactoryRegistration* factory =
            behaviors.FindTypeFactory(type.Id());
        if (factory != nullptr && factory->factory != nullptr) {
            const std::uint32_t index = factories_.Size();
            result = factories_.PushBack({type.Id(), factory->factory});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TypeFactory, index);
            if (!result) return result.GetStatus();
        }
        if (type.ContentMember() != InvalidMemberId) {
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    type.ContentMember());
            const PropertyInfo* descriptor =
                types.FindProperty(type.ContentMember());
            if (descriptor == nullptr) {
                return InvalidState(
                    "Content metadata references a missing property");
            }
            const bool collection =
                (static_cast<std::uint32_t>(descriptor->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Collection)) != 0U;
            const ContentKind kind = content != nullptr
                ? content->kind
                : (collection ? ContentKind::Collection : ContentKind::Single);
            const std::uint32_t index = contents_.Size();
            result = contents_.PushBack({
                type.Id(), type.ContentMember(), kind,
                content != nullptr ? content->flags : ContentFlags::None,
                content != nullptr ? content->write : nullptr,
                content != nullptr ? content->clear : nullptr,
                content != nullptr ? content->context : nullptr});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                type.ContentMember(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
        }
        for (const PropertyInfo& property :
             type.Properties()) {
            if (property.Id() == type.ContentMember()) {
                continue;
            }
            const ContentAccessorRegistration* content =
                behaviors.FindContentAccessor(
                    property.Id());
            if (content == nullptr) continue;
            const std::uint32_t index = contents_.Size();
            result = contents_.PushBack({
                type.Id(),
                property.Id(),
                content->kind,
                content->flags,
                content->write,
                content->clear,
                content->context});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                property.Id(), MetadataFacetKind::Content, index);
            if (!result) return result.GetStatus();
        }
        const PropertyChangeNotificationRegistration* notification =
            behaviors.FindPropertyChangeNotification(type.Id());
        if (notification != nullptr) {
            const std::uint32_t index =
                propertyChangeNotifications_.Size();
            result = propertyChangeNotifications_.PushBack({
                type.Id(),
                notification->subscribe,
                notification->unsubscribe,
                notification->context});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::PropertyChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }
        const CollectionChangeNotificationRegistration*
            collectionNotification =
                behaviors.FindCollectionChangeNotification(type.Id());
        if (collectionNotification != nullptr) {
            const std::uint32_t index =
                collectionChangeNotifications_.Size();
            result = collectionChangeNotifications_.PushBack({
                type.Id(),
                collectionNotification->subscribe,
                collectionNotification->unsubscribe,
                collectionNotification->context});
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::CollectionChangeNotification,
                index);
            if (!result) return result.GetStatus();
        }

        for (const PropertyInfo& property : type.Properties()) {
            const PropertyAccessorRegistration* accessor =
                behaviors.FindPropertyAccessor(property.Id());
            if (accessor != nullptr &&
                accessor->access != PropertyAccessKind::External) {
                const std::uint32_t index = propertyAccessors_.Size();
                result = propertyAccessors_.PushBack({
                    property.Id(), accessor->access, accessor->get,
                    accessor->set, accessor->provider, accessor->context});
                if (!result) return result.GetStatus();
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::PropertyAccessor,
                    index);
                if (!result) return result.GetStatus();
            }

            const DependencyProperty* dependency = dependencyProperties.Find(
                DependencyPropertyHandle{property.Id()});
            if (dependency != nullptr) {
                const std::uint32_t index = dependencyProperties_.Size();
                result = dependencyProperties_.PushBack({
                    property.Id(), dependency->Handle().value,
                    dependency->RegisteredOwnerType(), dependency->ValueType(),
                    static_cast<std::uint32_t>(dependency->Flags()),
                    dependency->MetadataCount(), dependency});
                if (!result) return result.GetStatus();
                result = SetMemberFacet(
                    property.Id(), MetadataFacetKind::DependencyProperty,
                    index);
                if (!result) return result.GetStatus();
            }
        }

        for (const FieldInfo& field : type.Fields()) {
            const ValueMemberAccessorRegistration* accessor =
                behaviors.FindValueMemberAccessor(field.Id());
            if (accessor == nullptr || accessor->get == nullptr) continue;
            const std::uint32_t index = valueMemberAccessors_.Size();
            result = valueMemberAccessors_.PushBack({
                field.Id(), accessor->get, accessor->set, accessor->context});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                field.Id(), MetadataFacetKind::ValueMemberAccessor, index);
            if (!result) return result.GetStatus();
        }

        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                behaviors.FindMethodInvoker(method.Id());
            if (invoker == nullptr || invoker->invoke == nullptr) continue;
            const std::uint32_t index = methodInvokers_.Size();
            result = methodInvokers_.PushBack({
                method.Id(), invoker->invoke, invoker->context});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                method.Id(), MetadataFacetKind::MethodInvoker, index);
            if (!result) return result.GetStatus();
        }

        for (const EventInfo& event : type.Events()) {
            if (!HasEventFlag(event.Flags(), EventFlags::Routed)) continue;
            const RoutedEventTable::Definition* definition =
                routedEvents.Find({event.Id()});
            if (definition == nullptr) {
                return InvalidState(
                    "Routed event metadata has no catalog definition");
            }
            const std::uint32_t index = routedEvents_.Size();
            result = routedEvents_.PushBack({
                event.Id(), event.OwnerType(), event.EventArgsType(),
                definition->strategy});
            if (!result) return result.GetStatus();
            result = SetMemberFacet(
                event.Id(), MetadataFacetKind::RoutedEvent, index);
            if (!result) return result.GetStatus();
        }
    }

    sealed_ = true;
    return {};
}

Base::Result<void> MetaTable::SealIndex() noexcept {
    typeRecords_.Clear();
    memberRecords_.Clear();
    facetRefs_.Clear();
    typeIndex_.Clear();
    memberIndex_.Clear();

    Base::Result<void> result = typeRecords_.Reserve(typeDrafts_.Size());
    if (!result) return result.GetStatus();
    result = memberRecords_.Reserve(memberDrafts_.Size());
    if (!result) return result.GetStatus();
    result = typeIndex_.Reserve(typeDrafts_.Size());
    if (!result) return result.GetStatus();
    result = memberIndex_.Reserve(memberDrafts_.Size());
    if (!result) return result.GetStatus();

    auto appendRefs = [this](
        const FacetDraft& draft,
        MetadataFacetMask& mask,
        std::uint16_t& count) noexcept -> Base::Result<void> {
        for (std::uint8_t kind = 0U; kind < 11U; ++kind) {
            if (draft.facets[kind] == InvalidFacetIndex) continue;
            Base::Result<void> added =
                facetRefs_.PushBack(draft.facets[kind]);
            if (!added) return added.GetStatus();
            mask |= UINT64_C(1) << kind;
            ++count;
        }
        return {};
    };

    for (const FacetDraft& draft : typeDrafts_) {
        TypeRecord record;
        record.id = static_cast<TypeId>(draft.key);
        record.firstFacetRef = facetRefs_.Size();
        result = appendRefs(draft, record.mask, record.facetCount);
        if (!result) return result.GetStatus();
        const std::uint32_t position = typeRecords_.Size();
        result = typeRecords_.PushBack(record);
        if (!result) return result.GetStatus();
        Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult>
            inserted = typeIndex_.Insert(record.id, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata type facet record is duplicated");
        }
    }
    for (const FacetDraft& draft : memberDrafts_) {
        MemberRecord record;
        record.id = static_cast<MemberId>(draft.key);
        record.firstFacetRef = facetRefs_.Size();
        result = appendRefs(draft, record.mask, record.facetCount);
        if (!result) return result.GetStatus();
        const std::uint32_t position = memberRecords_.Size();
        result = memberRecords_.PushBack(record);
        if (!result) return result.GetStatus();
        Base::Result<Base::HashMap<MemberId, std::uint32_t>::InsertResult>
            inserted = memberIndex_.Insert(record.id, position);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata member facet record is duplicated");
        }
    }
    typeDrafts_.Clear();
    memberDrafts_.Clear();
    return {};
}

MetadataFacetMask MetaTable::TypeFacets(TypeId type) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    return position != nullptr && *position < typeRecords_.Size()
        ? typeRecords_[*position].mask
        : 0U;
}

MetadataFacetMask MetaTable::MemberFacets(
    MemberId member) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    return position != nullptr && *position < memberRecords_.Size()
        ? memberRecords_[*position].mask
        : 0U;
}

std::uint32_t MetaTable::FindTypeFacet(
    TypeId type,
    MetadataFacetKind kind) const noexcept {
    const std::uint32_t* position = typeIndex_.Find(type);
    if (position == nullptr || *position >= typeRecords_.Size()) {
        return InvalidFacetIndex;
    }
    const TypeRecord& record = typeRecords_[*position];
    const MetadataFacetMask bit = MetadataFacetBit(kind);
    if ((record.mask & bit) == 0U) return InvalidFacetIndex;
    const std::uint32_t reference = record.firstFacetRef +
        FacetCountBefore(record.mask, kind);
    return reference < facetRefs_.Size()
        ? facetRefs_[reference]
        : InvalidFacetIndex;
}

std::uint32_t MetaTable::FindMemberFacet(
    MemberId member,
    MetadataFacetKind kind) const noexcept {
    const std::uint32_t* position = memberIndex_.Find(member);
    if (position == nullptr || *position >= memberRecords_.Size()) {
        return InvalidFacetIndex;
    }
    const MemberRecord& record = memberRecords_[*position];
    const MetadataFacetMask bit = MetadataFacetBit(kind);
    if ((record.mask & bit) == 0U) return InvalidFacetIndex;
    const std::uint32_t reference = record.firstFacetRef +
        FacetCountBefore(record.mask, kind);
    return reference < facetRefs_.Size()
        ? facetRefs_[reference]
        : InvalidFacetIndex;
}

const TypeFactoryFacet* MetaTable::FindTypeFactory(
    TypeId type) const noexcept {
    const std::uint32_t index = FindTypeFacet(
        type, MetadataFacetKind::TypeFactory);
    return index < factories_.Size() ? &factories_[index] : nullptr;
}

const ContentFacet* MetaTable::FindContent(TypeId type) const noexcept {
    const std::uint32_t index = FindTypeFacet(
        type, MetadataFacetKind::Content);
    return index < contents_.Size() ? &contents_[index] : nullptr;
}

const ContentFacet* MetaTable::FindContentByMember(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::Content);
    return index < contents_.Size() ? &contents_[index] : nullptr;
}

MemberId MetaTable::FindContentMember(TypeId type) const noexcept {
    if (types_ == nullptr) return InvalidMemberId;
    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_->TypeCount(); ++depth) {
        const ContentFacet* content = FindContent(current);
        if (content != nullptr) return content->member;
        const TypeInfo* descriptor = types_->FindType(current);
        if (descriptor == nullptr) return InvalidMemberId;
        current = descriptor->BaseType();
    }
    return InvalidMemberId;
}

const PropertyAccessorFacet* MetaTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::PropertyAccessor);
    return index < propertyAccessors_.Size()
        ? &propertyAccessors_[index] : nullptr;
}

const ValueMemberAccessorFacet* MetaTable::FindValueMemberAccessor(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::ValueMemberAccessor);
    return index < valueMemberAccessors_.Size()
        ? &valueMemberAccessors_[index] : nullptr;
}

const MethodInvokerFacet* MetaTable::FindMethodInvoker(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::MethodInvoker);
    return index < methodInvokers_.Size()
        ? &methodInvokers_[index] : nullptr;
}

const DependencyPropertyFacet* MetaTable::FindDependencyProperty(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::DependencyProperty);
    return index < dependencyProperties_.Size()
        ? &dependencyProperties_[index] : nullptr;
}

const RoutedEventFacet* MetaTable::FindRoutedEvent(
    MemberId member) const noexcept {
    const std::uint32_t index = FindMemberFacet(
        member, MetadataFacetKind::RoutedEvent);
    return index < routedEvents_.Size() ? &routedEvents_[index] : nullptr;
}

const PropertyChangeNotificationFacet*
MetaTable::FindPropertyChangeNotification(
    TypeId type) const noexcept {
    if (types_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const PropertyChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > types_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t index = FindTypeFacet(
            current, MetadataFacetKind::PropertyChangeNotification);
        if (index < propertyChangeNotifications_.Size()) {
            return &propertyChangeNotifications_[index];
        }
        const TypeInfo* descriptor =
            types_->FindType(current);
        if (descriptor == nullptr) return nullptr;
        for (TypeId interfaceType : descriptor->Interfaces()) {
            const PropertyChangeNotificationFacet* inherited =
                self(self, interfaceType, depth + 1U);
            if (inherited != nullptr) return inherited;
        }
        return self(self, descriptor->BaseType(), depth + 1U);
    };
    return find(find, type, 0U);
}

const CollectionChangeNotificationFacet*
MetaTable::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    if (types_ == nullptr) return nullptr;
    const auto find = [this](
        const auto& self,
        TypeId current,
        std::uint32_t depth) noexcept
        -> const CollectionChangeNotificationFacet* {
        if (current == InvalidTypeId ||
            depth > types_->TypeCount()) {
            return nullptr;
        }
        const std::uint32_t index = FindTypeFacet(
            current, MetadataFacetKind::CollectionChangeNotification);
        if (index < collectionChangeNotifications_.Size()) {
            return &collectionChangeNotifications_[index];
        }
        const TypeInfo* descriptor =
            types_->FindType(current);
        if (descriptor == nullptr) return nullptr;
        for (TypeId interfaceType : descriptor->Interfaces()) {
            const CollectionChangeNotificationFacet* inherited =
                self(self, interfaceType, depth + 1U);
            if (inherited != nullptr) return inherited;
        }
        return self(self, descriptor->BaseType(), depth + 1U);
    };
    return find(find, type, 0U);
}

Base::Result<Base::HashCode> MetaTable::ComputeHash() const noexcept {
    if (!sealed_) return InvalidState("Facet hash requires a sealed store");
    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.FACETS.V4";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    Base::Vector<std::uint32_t> order;
    Base::Result<void> result = BuildOrder(factories_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return factories_[left].type < factories_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) builder.AddU64(factories_[index].type);

    order.Clear();
    result = BuildOrder(contents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return contents_[left].type < contents_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ContentFacet& facet = contents_[index];
        builder.AddU64(facet.type);
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.kind));
        builder.AddByte(static_cast<std::uint8_t>(facet.flags));
        builder.AddByte(facet.write != nullptr ? 1U : 0U);
        builder.AddByte(facet.clear != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(propertyAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyAccessors_[left].member <
                propertyAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyAccessorFacet& facet = propertyAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(static_cast<std::uint8_t>(facet.access));
        builder.AddU64(facet.provider);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(valueMemberAccessors_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return valueMemberAccessors_[left].member <
                valueMemberAccessors_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const ValueMemberAccessorFacet& facet = valueMemberAccessors_[index];
        builder.AddU64(facet.member);
        builder.AddByte(facet.get != nullptr ? 1U : 0U);
        builder.AddByte(facet.set != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(methodInvokers_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return methodInvokers_[left].member < methodInvokers_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        builder.AddU64(methodInvokers_[index].member);
    }

    order.Clear();
    result = BuildOrder(dependencyProperties_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return dependencyProperties_[left].member <
                dependencyProperties_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const DependencyPropertyFacet& facet = dependencyProperties_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.canonicalMember);
        builder.AddU64(facet.registeredOwnerType);
        builder.AddU64(facet.valueType);
        builder.AddU32(facet.flags);
        builder.AddU32(facet.metadataCount);
    }

    order.Clear();
    result = BuildOrder(routedEvents_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return routedEvents_[left].member < routedEvents_[right].member;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const RoutedEventFacet& facet = routedEvents_[index];
        builder.AddU64(facet.member);
        builder.AddU64(facet.ownerType);
        builder.AddU64(facet.eventArgsType);
    }

    order.Clear();
    result = BuildOrder(propertyChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return propertyChangeNotifications_[left].type <
                propertyChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const PropertyChangeNotificationFacet& facet =
            propertyChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }

    order.Clear();
    result = BuildOrder(collectionChangeNotifications_.Size(), order,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return collectionChangeNotifications_[left].type <
                collectionChangeNotifications_[right].type;
        });
    if (!result) return result.GetStatus();
    builder.AddU32(order.Size());
    for (std::uint32_t index : order) {
        const CollectionChangeNotificationFacet& facet =
            collectionChangeNotifications_[index];
        builder.AddU64(facet.type);
        builder.AddByte(facet.subscribe != nullptr ? 1U : 0U);
        builder.AddByte(facet.unsubscribe != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero


// ===== BehaviorTable =====



namespace Aero::Meta {

Base::Result<void*>
BehaviorTable::OwnContextRaw(
    std::size_t size,
    std::size_t alignment,
    void* source,
    void (*construct)(void*, void*) noexcept,
    void (*destroyValue)(void*) noexcept) noexcept {
    if (size == 0U ||
        alignment == 0U ||
        source == nullptr ||
        construct == nullptr ||
        destroyValue == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata behavior context descriptor is invalid");
    }

    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        size,
        alignment,
        Base::MemoryTag::General});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Metadata behavior context allocation failed");
    }
    construct(memory, source);

    OwnedBehaviorData context;
    context.allocator = &allocator;
    context.value = memory;
    context.destroyValue = destroyValue;
    context.size = size;
    context.alignment = alignment;
    context.destroy =
        [](OwnedBehaviorData& owned) noexcept {
            owned.destroyValue(owned.value);
            owned.allocator->Deallocate(
                owned.value,
                owned.size,
                owned.alignment,
                Base::MemoryTag::General);
            owned = {};
        };
    Base::Result<void> retained =
        ownedContexts_.PushBack(context);
    if (!retained) {
        context.destroy(context);
        return retained.GetStatus();
    }
    return memory;
}
namespace {

bool IsValidPropertyBehavior(
    const PropertyAccessorRegistration& registration) noexcept {
    switch (registration.access) {
    case PropertyAccessKind::External:
        return registration.get == nullptr && registration.set == nullptr &&
            registration.provider == InvalidPropertyProviderId &&
            registration.context == nullptr;
    case PropertyAccessKind::Ordinary:
        return (registration.get != nullptr || registration.set != nullptr) &&
            registration.provider == InvalidPropertyProviderId;
    case PropertyAccessKind::Provider:
        return registration.provider != InvalidPropertyProviderId;
    }
    return false;
}

bool HasPropertyFlagValue(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

} // namespace

BehaviorTable::~BehaviorTable()
    noexcept {
    for (std::uint32_t index = ownedContexts_.Size();
         index > 0U; --index) {
        OwnedBehaviorData& context =
            ownedContexts_[index - 1U];
        if (context.destroy != nullptr) {
            context.destroy(context);
        }
    }
}

void BehaviorTable::ReleaseLastContext(
    void* value) noexcept {
    if (ownedContexts_.Empty() ||
        ownedContexts_.Back().value != value) {
        return;
    }
    OwnedBehaviorData& context = ownedContexts_.Back();
    if (context.destroy != nullptr) {
        context.destroy(context);
    }
    ownedContexts_.PopBack();
}

Base::Result<void> BehaviorTable::AdoptOwnedContextsFrom(
    BehaviorTable& source) noexcept {
    if (&source == this || source.ownedContexts_.Empty()) return {};
    Base::Result<void> reserved = ownedContexts_.Reserve(
        ownedContexts_.Size() + source.ownedContexts_.Size());
    if (!reserved) return reserved.GetStatus();
    for (OwnedBehaviorData& context : source.ownedContexts_) {
        Base::Result<void> added = ownedContexts_.PushBack(context);
        AERO_ASSERT(added);
        if (!added) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Reserved metadata context adoption unexpectedly failed");
        }
        context = {};
    }
    source.ownedContexts_.Clear();
    return {};
}

Base::Result<void> BehaviorTable::Freeze() noexcept {
    if (frozen_) return {};
    if (types_ == nullptr || !types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before behavior registrations");
    }

    for (const TypeFactoryRegistration& factory : typeFactories_) {
        const TypeInfo* type = types_->FindType(factory.type);
        if (factory.type == InvalidTypeId || factory.factory == nullptr ||
            type == nullptr || type->Kind() != MetadataTypeKind::Object) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Type factory registration is invalid");
        }
    }
    for (const ContentAccessorRegistration& content : contentAccessors_) {
        const TypeInfo* type = types_->FindType(content.type);
        const PropertyInfo* member = types_->FindProperty(content.member);
        const bool collection = member != nullptr &&
            HasPropertyFlagValue(
                member->Flags(), PropertyFlags::Collection);
        if (type == nullptr || type->Kind() != MetadataTypeKind::Object ||
            member == nullptr ||
            member->OwnerType() != content.type ||
            !HasPropertyFlagValue(
                member->Flags(), PropertyFlags::Structural) ||
            collection != (content.kind == ContentKind::Collection) ||
            content.write == nullptr || content.clear == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Content accessor registration is invalid");
        }
    }
    for (const PropertyAccessorRegistration& accessor : propertyAccessors_) {
        if (accessor.member == InvalidMemberId ||
            types_->FindProperty(accessor.member) == nullptr ||
            !IsValidPropertyBehavior(accessor)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property accessor registration is invalid");
        }
    }
    for (const ValueMemberAccessorRegistration& accessor :
         valueMemberAccessors_) {
        const FieldInfo* field = types_->FindField(accessor.member);
        if (accessor.member == InvalidMemberId || field == nullptr ||
            accessor.get == nullptr ||
            (!HasFieldFlag(field->Flags(), FieldFlags::ReadOnly) &&
             accessor.set == nullptr)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Value member accessor registration is invalid");
        }
    }
    for (const MethodInvokerRegistration& invoker : methodInvokers_) {
        if (invoker.member == InvalidMemberId || invoker.invoke == nullptr ||
            types_->FindMethod(invoker.member) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Method invoker registration is invalid");
        }
    }
    for (const PropertyChangeNotificationRegistration& notification :
         propertyChangeNotifications_) {
        const TypeInfo* type = types_->FindType(notification.type);
        if (type == nullptr ||
            (type->Kind() != MetadataTypeKind::Object &&
             type->Kind() != MetadataTypeKind::Interface) ||
            notification.subscribe == nullptr ||
            notification.unsubscribe == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Property-change notification registration is invalid");
        }
    }
    for (const CollectionChangeNotificationRegistration& notification :
         collectionChangeNotifications_) {
        const TypeInfo* type = types_->FindType(notification.type);
        if (type == nullptr ||
            (type->Kind() != MetadataTypeKind::Object &&
             type->Kind() != MetadataTypeKind::Interface) ||
            !HasTypeFlag(type->Flags(), TypeFlags::Collection) ||
            notification.subscribe == nullptr ||
            notification.unsubscribe == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Collection-change notification registration is invalid");
        }
    }
    for (const TypeInfo& type : types_->Types()) {
        for (const FieldInfo& field : type.Fields()) {
            const ValueMemberAccessorRegistration* accessor =
                FindValueMemberAccessor(field.Id());
            if (accessor == nullptr || accessor->get == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Registered value field has no accessor behavior");
            }
        }
        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                FindMethodInvoker(method.Id());
            if (invoker == nullptr || invoker->invoke == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Registered method has no invoker behavior");
            }
        }
    }

    frozen_ = true;
    return {};
}

const TypeFactoryRegistration*
BehaviorTable::FindTypeFactory(TypeId type) const noexcept {
    for (const TypeFactoryRegistration& registration : typeFactories_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const ContentAccessorRegistration*
BehaviorTable::FindContentAccessor(
    MemberId member) const noexcept {
    for (const ContentAccessorRegistration& registration : contentAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const PropertyAccessorRegistration*
BehaviorTable::FindPropertyAccessor(
    MemberId member) const noexcept {
    for (const PropertyAccessorRegistration& registration : propertyAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const ValueMemberAccessorRegistration*
BehaviorTable::FindValueMemberAccessor(
    MemberId member) const noexcept {
    for (const ValueMemberAccessorRegistration& registration :
         valueMemberAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const MethodInvokerRegistration*
BehaviorTable::FindMethodInvoker(
    MemberId member) const noexcept {
    for (const MethodInvokerRegistration& registration : methodInvokers_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const PropertyChangeNotificationRegistration*
BehaviorTable::FindPropertyChangeNotification(
    TypeId type) const noexcept {
    for (const PropertyChangeNotificationRegistration& registration :
         propertyChangeNotifications_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const CollectionChangeNotificationRegistration*
BehaviorTable::FindCollectionChangeNotification(
    TypeId type) const noexcept {
    for (const CollectionChangeNotificationRegistration& registration :
         collectionChangeNotifications_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

Base::Result<void> RegistrationTypes::ValidateRegistrationPair()
    const noexcept {
    if (types_ == nullptr || behaviors_ == nullptr ||
        &behaviors_->Types() != types_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata registration type and behavior stores do not match");
    }
    if (types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata type registry is frozen");
    }
    if (behaviors_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata behavior registration store is frozen");
    }
    return {};
}

Base::Result<TypeId> RegistrationTypes::RegisterType(
    const TypeRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterType(*behaviors_, registration);
}

Base::Result<void> RegistrationTypes::RegisterInterface(
    TypeId ownerType,
    TypeId interfaceType,
    InterfaceCastThunk cast) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterInterface(ownerType, interfaceType, cast);
}

Base::Result<MemberId> RegistrationTypes::RegisterProperty(
    TypeId ownerType,
    const PropertyRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterProperty(*behaviors_, ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterField(
    TypeId ownerType,
    const FieldRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterField(*behaviors_, ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterEnumValue(
    TypeId ownerType,
    const EnumValueRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterEnumValue(ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterEvent(
    TypeId ownerType,
    const EventRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterEvent(ownerType, registration);
}

Base::Result<MemberId> RegistrationTypes::RegisterMethod(
    TypeId ownerType,
    const MethodRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->RegisterMethod(*behaviors_, ownerType, registration);
}

Base::Result<void> RegistrationTypes::SetFactory(
    TypeId type,
    ObjectFactory factory) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->SetFactory(*behaviors_, type, factory);
}

Base::Result<void> RegistrationTypes::SetContentMember(
    TypeId type,
    MemberId member) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    return types_->SetContentMember(type, member);
}

Base::Result<void> RegistrationTypes::SetContentAccessor(
    const ContentAccessorRegistration& registration) const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid.GetStatus();
    const TypeInfo* type = types_->FindType(registration.type);
    const PropertyInfo* member = types_->FindProperty(registration.member);
    const bool collection = member != nullptr &&
        HasPropertyFlagValue(member->Flags(), PropertyFlags::Collection);
    if (type == nullptr || type->Kind() != MetadataTypeKind::Object ||
        member == nullptr ||
        member->OwnerType() != registration.type ||
        !HasPropertyFlagValue(
            member->Flags(), PropertyFlags::Structural) ||
        collection != (registration.kind == ContentKind::Collection) ||
        registration.write == nullptr || registration.clear == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Content accessor registration is invalid");
    }
    if (behaviors_->FindContentAccessor(registration.member) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Content accessor is already registered");
    }
    return behaviors_->contentAccessors_.PushBack(registration);
}

Base::Result<void>
RegistrationTypes::RegisterPropertyChangeNotification(
    const PropertyChangeNotificationRegistration& registration)
    const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid;
    const TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr ||
        (type->Kind() != MetadataTypeKind::Object &&
         type->Kind() != MetadataTypeKind::Interface) ||
        registration.subscribe == nullptr ||
        registration.unsubscribe == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property-change notification registration is invalid");
    }
    if (behaviors_->FindPropertyChangeNotification(
            registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Property-change notification is already registered");
    }
    return behaviors_->propertyChangeNotifications_.PushBack(
        registration);
}

Base::Result<void>
RegistrationTypes::RegisterCollectionChangeNotification(
    const CollectionChangeNotificationRegistration& registration)
    const noexcept {
    Base::Result<void> valid = ValidateRegistrationPair();
    if (!valid) return valid;
    const TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr ||
        (type->Kind() != MetadataTypeKind::Object &&
         type->Kind() != MetadataTypeKind::Interface) ||
        !HasTypeFlag(type->Flags(), TypeFlags::Collection) ||
        registration.subscribe == nullptr ||
        registration.unsubscribe == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Collection-change notification registration is invalid");
    }
    if (behaviors_->FindCollectionChangeNotification(
            registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Collection-change notification is already registered");
    }
    return behaviors_->collectionChangeNotifications_.PushBack(
        registration);
}

} // namespace Aero::Meta


// ===== Registry =====



#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>

#include <new>

namespace Aero::Meta {

struct Registry::Storage {
    struct ModuleRecord {
        MetadataModuleId id = InvalidMetadataModuleId;
        std::uint32_t schemaVersion = 1U;
        MetadataModuleRegisterCallback registerModule = nullptr;
        MetadataModuleRegisterContextCallback registerModuleWithContext =
            nullptr;
        void* context = nullptr;
        Base::String name;
    };

    TypeRegistry types;
    BehaviorTable behaviorRegistrations;
    ValueTable valueRegistrations;
    DependencyPropertyRegistry dependencyProperties;
    RoutedEventTable routedEvents;
    ::Aero::MetaTable facets;
    Base::Vector<ModuleRecord> modules;
    Base::Vector<MetadataPropertyProviderRegistration> providers;
    bool sealed = false;
    bool ready = false;

    Storage() noexcept
        : types(),
          behaviorRegistrations(types),
          valueRegistrations(types),
          dependencyProperties(types, behaviorRegistrations),
          routedEvents(types, behaviorRegistrations),
          facets(),
          modules(),
          providers() {}
};

Registry::Registry() noexcept
    : storage_(new (std::nothrow) Storage()) {}

Registry::~Registry() noexcept {
    delete storage_;
    storage_ = nullptr;
}

Base::Status Registry::OutOfMemoryStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory,
        "Registry storage allocation failed");
}

bool Registry::IsValid() const noexcept {
    return storage_ != nullptr;
}

bool Registry::IsSealed() const noexcept {
    return storage_ != nullptr && storage_->sealed;
}

std::uint32_t Registry::ModuleCount() const noexcept {
    return storage_ != nullptr ? storage_->modules.Size() : 0U;
}

Base::Result<void> Registry::ValidateRegistration(
    const MetadataModuleRegistration& registration) noexcept {
    if (registration.id == InvalidMetadataModuleId ||
        registration.name.Empty() || registration.schemaVersion == 0U ||
        (registration.registerModule == nullptr &&
         registration.registerModuleWithContext == nullptr) ||
        (registration.registerModule != nullptr &&
         registration.registerModuleWithContext != nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata module registration is incomplete");
    }
    if (registration.id != MakeMetadataModuleId(registration.name)) {
        return Base::Status::Failure(
            Base::ErrorCode::IdCollision,
            "Metadata module id does not match its stable name");
    }
    return {};
}

Base::Result<Registry::Storage*> Registry::BuildCandidate(
    const MetadataModuleRegistration* extra,
    bool seal) const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();

    Storage* candidate = new (std::nothrow) Storage();
    if (candidate == nullptr) return OutOfMemoryStatus();

    auto fail = [candidate](const Base::Status& status) noexcept
        -> Base::Result<Storage*> {
        delete candidate;
        return status;
    };
    RegistrationTypes registrations(
        candidate->types,
        candidate->behaviorRegistrations);

    // Clone the committed authoring state directly. Module callbacks are
    // intentionally not replayed: each callback executes exactly once and a
    // failed candidate leaves the current storage untouched.
    for (const TypeInfo& type : storage_->types.Types()) {
        const TypeFactoryRegistration* factory =
            storage_->behaviorRegistrations.FindTypeFactory(type.Id());
        const TypeRegistration registration{
            type.XamlNamespace(),
            type.Name(),
            type.BaseType(),
            type.Flags(),
            factory != nullptr ? factory->factory : nullptr,
            type.Kind(),
            type.UnderlyingType(),
            type.Interfaces()};
        Base::Result<TypeId> registered =
            registrations.RegisterType(registration);
        if (!registered) return fail(registered.GetStatus());
    }

    // Enum values are structural prerequisites for dependency-property
    // default-value validation, so restore them before cloning properties.
    for (const TypeInfo& type : storage_->types.Types()) {
        for (const EnumValueInfo& value : type.EnumValues()) {
            Base::Result<MemberId> registered =
                registrations.RegisterEnumValue(type.Id(), {
                    value.Name(), value.RawValue()});
            if (!registered) return fail(registered.GetStatus());
        }
    }

    // Value semantics are prerequisites for cloning any property default or
    // behavior payload that stores a non-inline value.  Restore them before
    // property registrations so a second registry candidate observes the
    // same value codec domain as the committed registry.
    for (const ValueTable::ValueSemanticsEntry& semantics :
         storage_->valueRegistrations.valueSemantics_) {
        Base::Result<void> cloned =
            candidate->valueRegistrations.RegisterValueSemantics(
                semantics.type,
                semantics.semantics->Registration());
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const TextValueConverterRegistration& converter :
         storage_->valueRegistrations.textConverters_) {
        Base::Result<void> cloned =
            candidate->valueRegistrations.RegisterTextConverter(converter);
        if (!cloned) return fail(cloned.GetStatus());
    }

    // Dependency properties create both their catalog entry and structural
    // property descriptor, so they are cloned before ordinary members.
    for (const Meta::DependencyProperty& property :
         storage_->dependencyProperties.properties_) {
        const Meta::DependencyProperty::MetadataEntry* ownerMetadata = nullptr;
        for (const Meta::DependencyProperty::MetadataEntry& entry :
             property.metadata_) {
            if (entry.owner &&
                entry.forType == property.registeredOwnerType_) {
                ownerMetadata = &entry;
                break;
            }
        }
        if (ownerMetadata == nullptr) {
            return fail(Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Dependency property clone is missing owner metadata"));
        }
        Base::Result<DependencyPropertyRegistrationResult> registered =
            candidate->dependencyProperties.Register({
                property.Name(),
                property.registeredOwnerType_,
                property.valueType_,
                property.flags_,
                ownerMetadata->metadata});
        if (!registered) return fail(registered.GetStatus());
        const DependencyPropertyHandle handle = registered.Value().property;
        for (const Meta::DependencyProperty::MetadataEntry& entry :
             property.metadata_) {
            if (&entry == ownerMetadata) continue;
            Base::Result<void> metadata = entry.owner
                ? candidate->dependencyProperties.AddOwner(
                      handle, entry.forType, entry.metadata)
                : candidate->dependencyProperties.OverrideMetadata(
                      handle, entry.forType, entry.metadata);
            if (!metadata) return fail(metadata.GetStatus());
        }
    }

    // Routed-event registration likewise owns the structural event record.
    for (const RoutedEventTable::Definition& definition :
         storage_->routedEvents.definitions_) {
        Base::Result<RoutedEventHandle> registered =
            candidate->routedEvents.Register({
                definition.name.View(),
                definition.ownerType,
                definition.eventArgsType,
                definition.strategy});
        if (!registered) return fail(registered.GetStatus());
    }

    for (const TypeInfo& type : storage_->types.Types()) {
        for (const PropertyInfo& property : type.Properties()) {
            if (candidate->types.FindProperty(property.Id()) != nullptr) {
                continue;
            }
            const PropertyAccessorRegistration* accessor =
                storage_->behaviorRegistrations.FindPropertyAccessor(
                    property.Id());
            PropertyRegistration registration;
            registration.name = property.Name();
            registration.valueType = property.ValueType();
            registration.flags = property.Flags();
            if (accessor != nullptr) {
                registration.access = accessor->access;
                registration.get = accessor->get;
                registration.set = accessor->set;
                registration.provider = accessor->provider;
                registration.context = accessor->context;
            }
            Base::Result<MemberId> registered =
                registrations.RegisterProperty(type.Id(), registration);
            if (!registered) return fail(registered.GetStatus());
        }
        for (const FieldInfo& field : type.Fields()) {
            const ValueMemberAccessorRegistration* accessor =
                storage_->behaviorRegistrations.FindValueMemberAccessor(
                    field.Id());
            if (accessor == nullptr) {
                return fail(Base::Status::Failure(
                    Base::ErrorCode::InternalError,
                    "Metadata field clone is missing its accessor"));
            }
            Base::Result<MemberId> registered =
                registrations.RegisterField(type.Id(), {
                    field.Name(), field.ValueType(), field.Flags(),
                    accessor->get, accessor->set, accessor->context});
            if (!registered) return fail(registered.GetStatus());
        }
        for (const EventInfo& event : type.Events()) {
            if (candidate->types.FindEvent(event.Id()) != nullptr) continue;
            Base::Result<MemberId> registered =
                registrations.RegisterEvent(type.Id(), {
                    event.Name(), event.EventArgsType(), event.Flags()});
            if (!registered) return fail(registered.GetStatus());
        }
        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                storage_->behaviorRegistrations.FindMethodInvoker(
                    method.Id());
            if (invoker == nullptr) {
                return fail(Base::Status::Failure(
                    Base::ErrorCode::InternalError,
                    "Metadata method clone is missing its invoker"));
            }
            Base::Vector<MethodParameterRegistration> parameters;
            Base::Result<void> reserved =
                parameters.Reserve(method.Parameters().Size());
            if (!reserved) return fail(reserved.GetStatus());
            for (const MethodParameterInfo& parameter : method.Parameters()) {
                Base::Result<void> added = parameters.PushBack({
                    parameter.Name(), parameter.Type()});
                if (!added) return fail(added.GetStatus());
            }
            Base::Result<MemberId> registered =
                registrations.RegisterMethod(type.Id(), {
                    method.Name(), method.ReturnType(),
                    {parameters.Data(), parameters.Size()}, method.Flags(),
                    invoker->invoke, invoker->context});
            if (!registered) return fail(registered.GetStatus());
        }
    }

    for (const TypeInfo& type : storage_->types.Types()) {
        if (type.ContentMember() == InvalidMemberId) continue;
        Base::Result<void> content = registrations.SetContentMember(
            type.Id(), type.ContentMember());
        if (!content) return fail(content.GetStatus());
    }
    for (const ContentAccessorRegistration& content :
         storage_->behaviorRegistrations.contentAccessors_) {
        Base::Result<void> cloned =
            registrations.SetContentAccessor(content);
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const PropertyChangeNotificationRegistration& notification :
         storage_->behaviorRegistrations.propertyChangeNotifications_) {
        Base::Result<void> cloned =
            registrations.RegisterPropertyChangeNotification(notification);
        if (!cloned) return fail(cloned.GetStatus());
    }
    for (const CollectionChangeNotificationRegistration& notification :
         storage_->behaviorRegistrations.collectionChangeNotifications_) {
        Base::Result<void> cloned =
            registrations.RegisterCollectionChangeNotification(
                notification);
        if (!cloned) return fail(cloned.GetStatus());
    }

    for (const Storage::ModuleRecord& source : storage_->modules) {
        Storage::ModuleRecord record;
        record.id = source.id;
        record.schemaVersion = source.schemaVersion;
        Base::Result<void> assigned = record.name.Assign(source.name.View());
        if (!assigned) return fail(assigned.GetStatus());
        Base::Result<void> added =
            candidate->modules.PushBack(std::move(record));
        if (!added) return fail(added.GetStatus());
    }

    auto applyAndAppend = [candidate](
        const MetadataModuleRegistration& registration) noexcept
        -> Base::Result<void> {
        ::Aero::RegistrationState contextState{
            &candidate->types,
            &candidate->behaviorRegistrations,
            &candidate->valueRegistrations,
            &candidate->dependencyProperties,
            &candidate->routedEvents};
        Registration context(&contextState);
        Base::Result<void> applied = registration.registerModule != nullptr
            ? registration.registerModule(context)
            : registration.registerModuleWithContext(
                  context, registration.context);
        if (!applied) return applied.GetStatus();

        Storage::ModuleRecord record;
        record.id = registration.id;
        record.schemaVersion = registration.schemaVersion;
        Base::Result<void> assigned = record.name.Assign(registration.name);
        if (!assigned) return assigned.GetStatus();
        return candidate->modules.PushBack(std::move(record));
    };

    if (extra != nullptr) {
        Base::Result<void> applied = applyAndAppend(*extra);
        if (!applied) return fail(applied.GetStatus());
    }

    if (seal) {
        Base::Result<void> frozen = candidate->types.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->behaviorRegistrations.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->valueRegistrations.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->dependencyProperties.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->routedEvents.Freeze();
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->facets.Build(
            candidate->types,
            candidate->behaviorRegistrations,
            candidate->dependencyProperties,
            candidate->routedEvents);
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        frozen = candidate->facets.BuildValueFacets(
            candidate->valueRegistrations,
            candidate->types);
        if (!frozen) {
            return fail(frozen.GetStatus());
        }
        candidate->sealed = true;
    }

    return candidate;
}

Base::Result<void> Registry::RegisterModule(
    const MetadataModuleRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is sealed");
    }
    Base::Result<void> valid = ValidateRegistration(registration);
    if (!valid) return valid.GetStatus();
    for (const Storage::ModuleRecord& module : storage_->modules) {
        if (module.id == registration.id) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Metadata module is already registered");
        }
    }

    // Modules are accumulated only while the registry is mutable. Applying
    // directly keeps behavior contexts owned by their registering table and
    // avoids rebuilding the value-semantic store once per module. Seal() then
    // freezes this single registry into the immutable runtime snapshot.
    ::Aero::RegistrationState contextState{
        &storage_->types,
        &storage_->behaviorRegistrations,
        &storage_->valueRegistrations,
        &storage_->dependencyProperties,
        &storage_->routedEvents};
    Registration context(&contextState);
    Base::Result<void> applied = registration.registerModule != nullptr
        ? registration.registerModule(context)
        : registration.registerModuleWithContext(
              context, registration.context);
    if (!applied) return applied.GetStatus();

    Storage::ModuleRecord record;
    record.id = registration.id;
    record.schemaVersion = registration.schemaVersion;
    Base::Result<void> assigned = record.name.Assign(registration.name);
    if (!assigned) return assigned.GetStatus();
    return storage_->modules.PushBack(std::move(record));
}

Base::Result<void> Registry::Seal() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->sealed) return {};
    Base::Result<void> frozen = storage_->types.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->behaviorRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->valueRegistrations.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->dependencyProperties.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->routedEvents.Freeze();
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.Build(
        storage_->types,
        storage_->behaviorRegistrations,
        storage_->dependencyProperties,
        storage_->routedEvents);
    if (!frozen) return frozen.GetStatus();
    frozen = storage_->facets.BuildValueFacets(
        storage_->valueRegistrations, storage_->types);
    if (!frozen) return frozen.GetStatus();
    storage_->sealed = true;
    return {};
}

const TypeRegistry& Registry::Types() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->types;
}

DependencyPropertyRegistry& Registry::DependencyProperties() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

const DependencyPropertyRegistry& Registry::DependencyProperties() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->dependencyProperties;
}

void* Registry::RoutedEventState() noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return &storage_->routedEvents;
}

const ::Aero::MetaTable& Registry::RuntimeData() const noexcept {
    AERO_ASSERT(storage_ != nullptr);
    return storage_->facets;
}

Base::Result<Base::HashCode> Registry::ComputeSchemaHash() const noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (!storage_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry schema hash requires a sealed domain");
    }

    Base::Result<Base::HashCode> descriptorHash =
        storage_->types.ComputeHash();
    if (!descriptorHash) return descriptorHash.GetStatus();
    Base::Result<Base::HashCode> facetHash = storage_->facets.ComputeHash();
    if (!facetHash) return facetHash.GetStatus();
    Base::Result<Base::HashCode> valueFacetHash =
        ::Aero::ComputeMetadataValueFacetHash(
            storage_->facets, storage_->types);
    if (!valueFacetHash) return valueFacetHash.GetStatus();

    Base::HashCode hash = Base::MixHash64(
        descriptorHash.Value() ^ facetHash.Value());
    hash = Base::MixHash64(hash ^ valueFacetHash.Value());
    for (const Storage::ModuleRecord& module : storage_->modules) {
        hash = Base::MixHash64(hash ^ module.id);
        hash = Base::MixHash64(
            hash ^ static_cast<Base::HashCode>(module.schemaVersion));
    }
    return hash;
}

} // namespace Aero::Meta

// Executable metadata operations share the Registry storage and lifetime.

namespace Aero::Meta {

Base::Result<void> Registry::RegisterPropertyProvider(
    const MetadataPropertyProviderRegistration& registration) noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry is complete");
    }
    if (registration.id == InvalidPropertyProviderId ||
        registration.objectType == InvalidTypeId ||
        (registration.get == nullptr && registration.set == nullptr) ||
        !IsSealed() ||
        Types().FindType(registration.objectType) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property provider registration is invalid");
    }
    if (FindProvider(registration.id) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Metadata property provider is already registered");
    }
    return storage_->providers.PushBack(registration);
}

Base::Result<void> Registry::Complete() noexcept {
    if (storage_ == nullptr) return OutOfMemoryStatus();
    if (storage_->ready) return {};
    if (!IsSealed() ||
        !Types().IsFrozen() ||
        !RuntimeData().IsSealed() ||
        !RuntimeData().ValueFacetsSealed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Registry and all typed behavior must be sealed before Registry completion");
    }
    storage_->ready = true;
    return {};
}


bool Registry::IsReady() const noexcept {
    return storage_ != nullptr && storage_->ready && storage_->sealed;
}

bool Registry::CanReadProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->get != nullptr));
}

bool Registry::CanWriteProperty(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    return accessor != nullptr &&
        (accessor->access == PropertyAccessKind::Provider ||
         (accessor->access == PropertyAccessKind::Ordinary &&
          accessor->set != nullptr));
}

bool Registry::CanReadValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->get != nullptr;
}

bool Registry::CanWriteValueMember(
    MemberId member) const noexcept {
    if (!IsReady()) return false;
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    return accessor != nullptr && accessor->set != nullptr;
}

MemberId Registry::FindContentMember(
    TypeId type) const noexcept {
    return IsReady()
        ? Types().FindContentMember(type)
        : InvalidMemberId;
}

Base::Result<ContentInfo> Registry::GetContentInfo(
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata content member was not found");
    }
    return ContentInfo{
        content->type,
        content->member,
        content->kind,
        content->flags,
        content->write != nullptr,
        content->clear != nullptr};
}

Base::Result<void> Registry::WriteContent(
    Base::Object& owner,
    MemberId member,
    const Base::Ref<Base::Object>& value) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->write == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member is not writable");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    const bool acceptsAnyValue =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    if (!value || property == nullptr ||
        (!acceptsAnyValue &&
         !Types().IsAssignableFrom(
             property->ValueType(),
             value->RuntimeType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content value type is incompatible");
    }
    content->write(owner, value, content->context);
    return {};
}

Base::Result<void> Registry::ClearContent(
    Base::Object& owner,
    MemberId member) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::ContentFacet* content =
        RuntimeData().FindContentByMember(member);
    if (content == nullptr || content->clear == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata content member cannot be cleared");
    }
    const PropertyInfo* property =
        Types().FindProperty(member);
    const bool attached =
        property != nullptr &&
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::Attached);
    if (!attached &&
        !Types().IsAssignableFrom(
            content->type, owner.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata content owner type is incompatible");
    }
    content->clear(owner, content->context);
    return {};
}

Base::Result<std::uint64_t>
Registry::SubscribePropertyChanged(
    Base::Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->subscribe == nullptr) {
        return UINT64_C(0);
    }
    return notification->subscribe(
        object,
        callback,
        callbackContext,
        notification->context);
}

Base::Result<bool> Registry::UnsubscribePropertyChanged(
    Base::Object& object,
    std::uint64_t subscription) const noexcept {
    if (!IsReady()) return MetadataNotReady();
    if (subscription == 0U) return false;
    const ::Aero::PropertyChangeNotificationFacet* notification =
        RuntimeData().FindPropertyChangeNotification(
            object.RuntimeType());
    if (notification == nullptr ||
        notification->unsubscribe == nullptr) {
        return false;
    }
    return notification->unsubscribe(
        object,
        subscription,
        notification->context);
}

void* Registry::TryCastToInterface(
    Base::Object& object,
    TypeId interfaceType) const noexcept {
    if (!IsReady()) {
        return nullptr;
    }
    return Types().TryCastToInterface(object, interfaceType);
}

Base::Result<Base::Ref<Base::Object>>
Registry::CreateObject(TypeId type) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::TypeFactoryFacet* factory =
        RuntimeData().FindTypeFactory(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata type descriptor was not found");
    }
    if (descriptor->Kind() != MetadataTypeKind::Object ||
        HasTypeFlag(descriptor->Flags(), TypeFlags::Abstract) ||
        factory == nullptr ||
        factory->factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no constructible factory");
    }
    Base::Result<Base::Ref<Base::Object>> created =
        factory->factory();
    if (!created) return created.GetStatus();
    if (!created.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "Metadata factory returned a null object");
    }
    if (created.Value()->RuntimeType() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata factory returned the wrong runtime type");
    }
    return created;
}

Base::Result<Value> Registry::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    const ::Aero::ValueSemanticsFacet* behavior =
        RuntimeData().FindValueSemantics(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value type descriptor was not found");
    }
    if (!HasTypeFlag(descriptor->Flags(), TypeFlags::ValueType) ||
        behavior == nullptr ||
        !behavior->semantics) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no value semantics");
    }
    return Value::TryFromCustom(
        type, source, behavior->semantics);
}

Base::Result<Value> Registry::TryConvertText(
    TypeId type,
    Base::StringView text) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const TypeInfo* descriptor = Types().FindType(type);
    if (descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata text target descriptor was not found");
    }
    if (descriptor->Kind() == MetadataTypeKind::Enum) {
        return ConvertEnumText(*descriptor, text);
    }
    const ::Aero::TextConverterFacet* converter =
        RuntimeData().FindTextConverter(type);
    if (converter == nullptr ||
        converter->convert == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata type has no text converter");
    }
    Base::Result<Value> converted =
        converter->convert(type, text, converter->context);
    if (!converted) return converted.GetStatus();
    if (converted.Value().IsUnset() ||
        converted.Value().Type() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata text converter returned an incompatible value");
    }
    return converted;
}

Base::Result<Value> Registry::GetValueMember(
    const Value& owner,
    MemberId member) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const FieldInfo* field = Types().FindField(member);
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    if (field == nullptr ||
        accessor == nullptr ||
        accessor->get == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value field or accessor was not found");
    }
    if (owner.Kind() != ValueKind::Custom ||
        owner.Type() != field->OwnerType() ||
        owner.AsCustom() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is incompatible");
    }
    Base::Result<Value> result = accessor->get(
        owner.AsCustom(),
        const_cast<Registry&>(*this),
        accessor->context);
    if (!result) return result.GetStatus();
    if (result.Value().IsUnset() ||
        result.Value().Type() != field->ValueType()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field getter returned an incompatible value");
    }
    return result;
}

Base::Result<void> Registry::SetValueMember(
    Value& owner,
    MemberId member,
    const Value& value) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const FieldInfo* field = Types().FindField(member);
    const ::Aero::ValueMemberAccessorFacet* accessor =
        RuntimeData().FindValueMemberAccessor(member);
    if (field == nullptr || accessor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata value field or accessor was not found");
    }
    if (owner.Kind() != ValueKind::Custom ||
        owner.Type() != field->OwnerType() ||
        owner.AsCustom() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field target is incompatible");
    }
    if (HasFieldFlag(field->Flags(), FieldFlags::ReadOnly) ||
        accessor->set == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Read-only metadata value field cannot be written");
    }
    if (value.IsUnset() ||
        value.Type() != field->ValueType()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value field value type does not match");
    }
    Base::Result<Value> clone =
        TryCreateValue(owner.Type(), owner.AsCustom());
    if (!clone) return clone.GetStatus();
    owner = std::move(clone).Value();
    accessor->set(
        owner.MutableCustom(),
        value,
        const_cast<Registry&>(*this),
        accessor->context);
    return {};
}

Base::Result<Value> Registry::GetProperty(
    const Base::Object& object,
    MemberId member) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::WriteOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Write-only metadata property cannot be read");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    Base::Result<Value> value = UnsupportedProperty();
    if (accessor->access == PropertyAccessKind::Ordinary) {
        if (accessor->get == nullptr) {
            return UnsupportedProperty();
        }
        value = accessor->get(object, accessor->context);
    } else if (accessor->access ==
               PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            value = GetDependencyProperty(
                object, *property);
        } else {
            const MetadataPropertyProviderRegistration* provider =
                FindProvider(accessor->provider);
            if (provider == nullptr ||
                provider->get == nullptr ||
                !Types().IsAssignableFrom(
                    provider->objectType,
                    object.RuntimeType())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Readable metadata property provider was not found");
            }
            value = provider->get(
                object, *property, provider->context);
        }
    }
    if (!value) return value.GetStatus();
    if (value.Value().IsUnset() ||
        (!HasPropertyFlag(
             property->Flags(),
             PropertyFlags::AnyValue) &&
         value.Value().Type() != property->ValueType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata getter returned an incompatible value");
    }
    return value;
}

Base::Result<void> Registry::SetProperty(
    Base::Object& object,
    MemberId member,
    const Value& value) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const PropertyInfo* property =
        Types().FindProperty(member);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata property descriptor was not found");
    }
    Base::Result<void> target =
        ValidatePropertyTarget(object, *property);
    if (!target) return target.GetStatus();
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value is unset");
    }
    if (HasPropertyFlag(
            property->Flags(), PropertyFlags::ReadOnly)) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Read-only metadata property cannot be written");
    }
    const ::Aero::PropertyAccessorFacet* accessor =
        RuntimeData().FindPropertyAccessor(member);
    if (accessor == nullptr) return UnsupportedProperty();

    const bool acceptsAnyValue =
        HasPropertyFlag(
            property->Flags(),
            PropertyFlags::AnyValue);
    const bool objectAssignment =
        value.Kind() == ValueKind::Object &&
        Types().IsAssignableFrom(
            property->ValueType(), value.Type());
    if (!acceptsAnyValue &&
        value.Type() != property->ValueType() &&
        !objectAssignment) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value type does not match the descriptor");
    }
    if (!acceptsAnyValue &&
        !IsRegisteredEnumValue(
            property->ValueType(), value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Metadata enum property value is not registered");
    }
    if (accessor->access ==
        PropertyAccessKind::Ordinary) {
        if (accessor->set == nullptr) {
            return UnsupportedProperty();
        }
        accessor->set(object, value, accessor->context);
        return {};
    }
    if (accessor->access ==
        PropertyAccessKind::Provider) {
        if (accessor->provider ==
            DependencyPropertyProviderId) {
            return SetDependencyProperty(
                object, *property, value);
        }
        const MetadataPropertyProviderRegistration* provider =
            FindProvider(accessor->provider);
        if (provider == nullptr ||
            provider->set == nullptr ||
            !Types().IsAssignableFrom(
                provider->objectType,
                object.RuntimeType())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Writable metadata property provider was not found");
        }
        return provider->set(
            object, *property, value, provider->context);
    }
    return UnsupportedProperty();
}

Base::Result<Value> Registry::InvokeMethod(
    Base::Object& object,
    MemberId member,
    Base::Span<const Value> arguments) const noexcept {
    if (!storage_->ready) return MetadataNotReady();
    const MethodInfo* method = Types().FindMethod(member);
    const ::Aero::MethodInvokerFacet* invoker =
        RuntimeData().FindMethodInvoker(member);
    if (method == nullptr ||
        invoker == nullptr ||
        invoker->invoke == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Metadata method or invoker was not found");
    }
    if (!Types().IsAssignableFrom(
            method->OwnerType(), object.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Object type is incompatible with the metadata method");
    }
    if (arguments.Size() !=
        method->Parameters().Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata method argument count does not match");
    }
    for (std::uint32_t index = 0U;
         index < arguments.Size(); ++index) {
        if (arguments[index].IsUnset() ||
            arguments[index].Type() !=
                method->Parameters()[index].Type()) {
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
          result.Value().Type() !=
              method->ReturnType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata method returned an incompatible value");
    }
    return result;
}

bool Registry::HasPropertyFlag(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status Registry::MetadataNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Registry is not complete");
}

Base::Status Registry::UnsupportedProperty() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Metadata property has no usable accessor");
}

bool Registry::IsRegisteredEnumValue(
    TypeId type,
    const Value& value) const noexcept {
    const TypeInfo* info = Types().FindType(type);
    if (info == nullptr ||
        info->Kind() != MetadataTypeKind::Enum) {
        return true;
    }
    if (HasTypeFlag(
            info->Flags(), TypeFlags::SignedEnum)) {
        return value.Kind() ==
                ValueKind::SignedInteger &&
            Types().IsEnumValue(
                type,
                static_cast<std::uint64_t>(
                    value.AsSignedInteger()));
    }
    return value.Kind() ==
            ValueKind::UnsignedInteger &&
        Types().IsEnumValue(
            type, value.AsUnsignedInteger());
}

Base::Result<Value> Registry::ConvertEnumText(
    const TypeInfo& type,
    Base::StringView input) const noexcept {
    Base::StringView remaining =
        ::Aero::Base::ValueConversion::Trim(input);
    if (remaining.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Enum text is empty");
    }
    std::uint64_t raw = 0U;
    std::uint32_t tokenCount = 0U;
    while (!remaining.Empty()) {
        std::uint32_t split =
            remaining.SizeBytes();
        for (std::uint32_t index = 0U;
             index < remaining.SizeBytes(); ++index) {
            if (remaining[index] == ',' ||
                remaining[index] == '|') {
                split = index;
                break;
            }
        }
        const Base::StringView token =
            ::Aero::Base::ValueConversion::Trim(
                remaining.Substr(0U, split));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text contains an empty value");
        }
        const EnumValueInfo* match = nullptr;
        for (const EnumValueInfo& candidate :
             type.EnumValues()) {
            if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
                    token, candidate.Name())) {
                match = &candidate;
                break;
            }
        }
        if (match == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Enum text value is not registered");
        }
        ++tokenCount;
        if (!type.IsFlagsEnum() &&
            tokenCount > 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Non-flags enum accepts exactly one value");
        }
        raw = type.IsFlagsEnum()
            ? (raw | match->RawValue())
            : match->RawValue();
        if (split == remaining.SizeBytes()) break;
        remaining = ::Aero::Base::ValueConversion::Trim(
            remaining.Substr(split + 1U));
    }
    return HasTypeFlag(
               type.Flags(), TypeFlags::SignedEnum)
        ? Value::FromSignedInteger(
              type.Id(),
              static_cast<std::int64_t>(raw))
        : Value::FromUnsignedInteger(type.Id(), raw);
}

Base::Result<void> Registry::ValidatePropertyTarget(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    if (HasPropertyFlag(
            property.Flags(), PropertyFlags::Attached)) {
        if (object.RuntimeType() != InvalidTypeId &&
            Types().FindType(
                object.RuntimeType()) != nullptr) {
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Attached metadata property target has no type information");
    }
    if (Types().IsAssignableFrom(
            property.OwnerType(),
            object.RuntimeType())) {
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Object type is incompatible with the metadata property");
}

Base::Result<Value> Registry::GetDependencyProperty(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    const auto& dependencyObject =
        static_cast<const DependencyObject&>(object);
    return dependencyObject.GetValue(handle);
}

Base::Result<void> Registry::SetDependencyProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value) const noexcept {
    const DependencyPropertyHandle handle{property.Id()};
    const DependencyPropertyRegistry& registry =
        DependencyProperties();
    if (!Types().IsAssignableFrom(
            TypeOf<DependencyObject>(),
            object.RuntimeType()) ||
        registry.Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata target is invalid");
    }
    auto& dependencyObject =
        static_cast<DependencyObject&>(object);
    dependencyObject.SetValue(handle, value);
    return {};
}

const MetadataPropertyProviderRegistration*
Registry::FindProvider(
    PropertyProviderId id) const noexcept {
    for (const MetadataPropertyProviderRegistration& provider :
         storage_->providers) {
        if (provider.id == id) return &provider;
    }
    return nullptr;
}

} // namespace Aero::Meta

// ===== Registration =====

namespace Aero::Meta {

namespace {

::Aero::RegistrationState& State(void* value) noexcept {
    return *static_cast<::Aero::RegistrationState*>(value);
}

} // namespace

RegistrationTypes Registration::Types() noexcept {
    ::Aero::RegistrationState& state = State(state_);
    return RegistrationTypes(
        *state.types, *state.behaviors);
}

RegistrationValues Registration::Values() noexcept {
    return RegistrationValues(
        State(state_).values,
        State(state_).values);
}

RegistrationValues Registration::Values() const noexcept {
    return RegistrationValues(
        State(state_).values,
        nullptr);
}

ValueTable&
Registration::ValueRegistrations() noexcept {
    return *State(state_).values;
}

DependencyPropertyRegistry&
Registration::DependencyProperties() noexcept {
    return *State(state_).properties;
}

} // namespace Aero::Meta


// ===== MetadataValueFacets =====



namespace Aero {
namespace {

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

Base::Status ValueFacetStateError(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> MetaTable::BuildValueFacets(
    const ValueTable& source,
    const TypeRegistry& types) noexcept {
    if (!sealed_ || valueFacetsSealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            valueFacetsSealed_
                ? "Metadata value facets are already sealed"
                : "Core metadata facets must be built before value facets");
    }
    if (!source.IsFrozen() || !types.IsFrozen() ||
        types_ != &types) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata value facet sources are not sealed or do not match");
    }

    std::uint32_t valueTypeCount = 0U;
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) ++valueTypeCount;
    }
    Base::Result<void> result = valueSemantics_.Reserve(valueTypeCount);
    if (!result) return result.GetStatus();
    result = textConverters_.Reserve(valueTypeCount);
    if (!result) return result.GetStatus();
    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) {
            const Base::Ref<ValueTypeSemantics>* semantics =
                source.FindValueSemantics(type.Id());
            if (semantics != nullptr &&
                semantics->Get() != nullptr) {
                ValueSemanticsFacet facet;
                facet.type = type.Id();
                facet.semantics = *semantics;
                const std::uint32_t index =
                    valueSemantics_.Size();
                result = valueSemantics_.PushBack(
                    std::move(facet));
                if (!result) return result.GetStatus();
                result = SetTypeFacet(
                    type.Id(), MetadataFacetKind::ValueSemantics, index);
                if (!result) return result.GetStatus();
            }
        }

        const TextValueConverterRegistration* converter =
            source.FindTextConverter(type.Id());
        if (converter != nullptr && converter->convert != nullptr) {
            TextConverterFacet facet;
            facet.type = type.Id();
            facet.convert = converter->convert;
            facet.context = converter->context;
            const std::uint32_t index = textConverters_.Size();
            result = textConverters_.PushBack(facet);
            if (!result) return result.GetStatus();
            result = SetTypeFacet(
                type.Id(), MetadataFacetKind::TextConverter, index);
            if (!result) return result.GetStatus();
        }
    }

    result = SealIndex();
    if (!result) return result.GetStatus();
    valueFacetsSealed_ = true;
    return {};
}

const ValueSemanticsFacet* MetaTable::FindValueSemantics(
    TypeId type) const noexcept {
    const std::uint32_t index = FindTypeFacet(
        type, MetadataFacetKind::ValueSemantics);
    return index < valueSemantics_.Size()
        ? &valueSemantics_[index] : nullptr;
}

const TextConverterFacet* MetaTable::FindTextConverter(
    TypeId type) const noexcept {
    const std::uint32_t index = FindTypeFacet(
        type, MetadataFacetKind::TextConverter);
    return index < textConverters_.Size()
        ? &textConverters_[index] : nullptr;
}

Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetaTable& facets,
    const TypeRegistry& descriptors) noexcept {
    if (!facets.IsSealed() || !facets.ValueFacetsSealed() ||
        !descriptors.IsFrozen()) {
        return ValueFacetStateError(
            "Value facet hash requires sealed descriptors and facets");
    }

    Base::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.VALUE.FACETS.V1";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    std::uint32_t semanticsCount = 0U;
    std::uint32_t converterCount = 0U;
    for (const TypeInfo& type : descriptors.Types()) {
        if (facets.FindValueSemantics(type.Id()) != nullptr) ++semanticsCount;
        if (facets.FindTextConverter(type.Id()) != nullptr) ++converterCount;
    }
    builder.AddU32(semanticsCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const ValueSemanticsFacet* facet =
            facets.FindValueSemantics(type.Id());
        if (facet == nullptr || !facet->semantics) continue;
        const ValueTypeRegistration& registration =
            facet->semantics->Registration();
        builder.AddU64(type.Id());
        builder.AddU32(registration.size);
        builder.AddU32(registration.alignment);
        builder.AddByte(registration.copy != nullptr ? 1U : 0U);
        builder.AddByte(registration.destroy != nullptr ? 1U : 0U);
        builder.AddByte(registration.equals != nullptr ? 1U : 0U);
        builder.AddByte(registration.inlineSafe ? 1U : 0U);
    }

    builder.AddU32(converterCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const TextConverterFacet* facet =
            facets.FindTextConverter(type.Id());
        if (facet == nullptr) continue;
        builder.AddU64(type.Id());
        builder.AddByte(facet->convert != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero

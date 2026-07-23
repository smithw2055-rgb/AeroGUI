#include <Aero/Core/TypeRegistry.hpp>

#include <Aero/Base/Assert.hpp>

#include <cstddef>
#include <cstdint>
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
        "Base, property value, or event argument type is not registered");
}

constexpr Base::Status InheritanceCycleStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::CycleDetected,
        "Type inheritance cycle detected");
}

constexpr Base::Status SnapshotBeforeFreezeStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "TypeRegistry snapshot requires a frozen registry");
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

Base::Result<void> Append(
    Base::String& output,
    Base::StringView text) noexcept {
    return output.TryAppendUnchecked(text);
}

Base::Result<void> AppendHex(
    Base::String& output,
    std::uint64_t value,
    std::uint32_t digits) noexcept {
    AERO_ASSERT(digits > 0U && digits <= 16U);
    static constexpr char HexDigits[] = "0123456789ABCDEF";
    char buffer[16]{};
    for (std::uint32_t index = 0U; index < digits; ++index) {
        const std::uint32_t target = digits - 1U - index;
        buffer[target] = HexDigits[
            static_cast<std::size_t>((value >> (index * 4U)) & 0xFU)];
    }
    return Append(output, Base::StringView(buffer, digits));
}

Base::Result<void> AppendSizedText(
    Base::String& output,
    Base::StringView text) noexcept {
    Base::Result<void> result = AppendHex(
        output, static_cast<std::uint64_t>(text.SizeBytes()), 8U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView(":"));
    if (!result) return result.GetStatus();
    return Append(output, text);
}

template<class Less>
Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> result = order.TryReserve(count);
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        result = order.TryPushBack(index);
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

Base::Result<void> AppendTypeLine(
    Base::String& output,
    const TypeInfo& type) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("T|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, type.Id(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, type.BaseType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(
        output, static_cast<std::uint32_t>(type.Flags()), 8U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, type.ContentMember(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendSizedText(output, type.XamlNamespace());
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendSizedText(output, type.Name());
    if (!result) return result.GetStatus();
    return Append(output, Base::StringView("\n"));
}

Base::Result<void> AppendPropertyLine(
    Base::String& output,
    const TypeRegistry& registry,
    const PropertyInfo& property) noexcept {
    const PropertyAccessorRegistration* accessor =
        registry.FindPropertyAccessor(property.Id());
    const PropertyAccessKind access = accessor != nullptr
        ? accessor->access : PropertyAccessKind::External;
    const PropertyProviderId provider = accessor != nullptr
        ? accessor->provider : InvalidPropertyProviderId;

    Base::Result<void> result = Append(output, Base::StringView("P|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, property.Id(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, property.OwnerType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, property.ValueType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(
        output, static_cast<std::uint32_t>(property.Flags()), 8U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, static_cast<std::uint32_t>(access), 2U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, provider, 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendSizedText(output, property.Name());
    if (!result) return result.GetStatus();
    return Append(output, Base::StringView("\n"));
}

Base::Result<void> AppendEventLine(
    Base::String& output,
    const EventInfo& eventInfo) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("E|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, eventInfo.Id(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, eventInfo.OwnerType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, eventInfo.EventArgsType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(
        output, static_cast<std::uint32_t>(eventInfo.Flags()), 8U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendSizedText(output, eventInfo.Name());
    if (!result) return result.GetStatus();
    return Append(output, Base::StringView("\n"));
}

Base::Result<void> AppendMethodLine(
    Base::String& output,
    const MethodInfo& method) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("M|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, method.Id(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, method.OwnerType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, method.ReturnType(), 16U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, static_cast<std::uint32_t>(method.Flags()), 8U);
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendSizedText(output, method.Name());
    if (!result) return result.GetStatus();
    result = Append(output, Base::StringView("|"));
    if (!result) return result.GetStatus();
    result = AppendHex(output, method.Parameters().Size(), 8U);
    if (!result) return result.GetStatus();
    for (const MethodParameterInfo& parameter : method.Parameters()) {
        result = Append(output, Base::StringView("|"));
        if (!result) return result.GetStatus();
        result = AppendHex(output, parameter.Type(), 16U);
        if (!result) return result.GetStatus();
        result = Append(output, Base::StringView(":"));
        if (!result) return result.GetStatus();
        result = AppendSizedText(output, parameter.Name());
        if (!result) return result.GetStatus();
    }
    return Append(output, Base::StringView("\n"));
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
      memberIndex_(),
      typeFactories_(),
      propertyAccessors_(),
      methodInvokers_(),
      valueSemantics_(),
      textConverters_() {}

Base::Result<TypeId> TypeRegistry::TryRegisterType(
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

    if (registration.factory != nullptr) {
        Base::Result<void> reserved = typeFactories_.TryReserve(
            typeFactories_.Size() + 1U);
        if (!reserved) return reserved.GetStatus();
    }

    TypeInfo info;
    info.id_ = id;
    info.baseType_ = registration.baseType;
    info.flags_ = registration.flags;
    Base::Result<void> result =
        info.xamlNamespace_.TryAssign(registration.xamlNamespace);
    if (!result) return result.GetStatus();
    result = info.name_.TryAssign(registration.name);
    if (!result) return result.GetStatus();

    const std::uint32_t index = types_.Size();
    result = types_.TryPushBack(std::move(info));
    if (!result) return result.GetStatus();

    bool factoryAdded = false;
    if (registration.factory != nullptr) {
        result = typeFactories_.TryPushBack({id, registration.factory});
        if (!result) {
            types_.PopBack();
            return result.GetStatus();
        }
        factoryAdded = true;
    }

    Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
        typeIndex_.TryInsert(id, index);
    if (!inserted || !inserted.Value().inserted) {
        if (factoryAdded) typeFactories_.PopBack();
        types_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterProperty(
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
        Base::Result<void> reserved = propertyAccessors_.TryReserve(
            propertyAccessors_.Size() + 1U);
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
        result = propertyAccessors_.TryPushBack({
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
        if (behaviorAdded) propertyAccessors_.PopBack();
        owner.properties_.PopBack();
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

    result = methodInvokers_.TryReserve(methodInvokers_.Size() + 1U);
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

    result = methodInvokers_.TryPushBack(
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
        methodInvokers_.PopBack();
        owner.methods_.PopBack();
        return !inserted ? inserted.GetStatus() : IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::TrySetFactory(
    TypeId type,
    ObjectFactory factory) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (FindType(type) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Factory owner type was not found");
    }
    if (factory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Type factory must not be null");
    }
    if (FindTypeFactory(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Type factory is already registered");
    }
    return typeFactories_.TryPushBack({type, factory});
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

Base::Result<void> TypeRegistry::TryRegisterValueSemantics(
    TypeId type,
    const ValueTypeRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    const TypeInfo* info = FindType(type);
    if (info == nullptr) return MissingRelatedTypeStatus();
    if ((static_cast<std::uint32_t>(info->Flags()) &
            static_cast<std::uint32_t>(TypeFlags::ValueType)) == 0U ||
        registration.size == 0U || registration.alignment == 0U ||
        !Base::IsValidAlignment(registration.alignment) ||
        registration.equals == nullptr ||
        (registration.inlineSafe &&
            (registration.size > Value::InlineCapacity ||
             registration.alignment > alignof(std::max_align_t))) ||
        (!registration.inlineSafe && registration.copy == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Value type semantics are invalid");
    }
    if (FindValueSemantics(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Value type semantics are already registered");
    }
    Base::Result<Base::Ref<ValueTypeSemantics>> created =
        Base::MakeRef<ValueTypeSemantics>(registration);
    if (!created) return created.GetStatus();
    return valueSemantics_.TryPushBack({type, std::move(created).Value()});
}

Base::Result<void> TypeRegistry::TryRegisterTextConverter(
    const TextValueConverterRegistration& registration) noexcept {
    if (frozen_) return RegistryFrozenStatus();
    if (registration.type == InvalidTypeId || registration.convert == nullptr ||
        FindType(registration.type) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text value converter registration is invalid");
    }
    if (FindTextConverter(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Text value converter is already registered");
    }
    return textConverters_.TryPushBack(registration);
}

const TypeFactoryRegistration* TypeRegistry::FindTypeFactory(
    TypeId type) const noexcept {
    for (const TypeFactoryRegistration& registration : typeFactories_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const PropertyAccessorRegistration* TypeRegistry::FindPropertyAccessor(
    MemberId member) const noexcept {
    for (const PropertyAccessorRegistration& registration : propertyAccessors_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

const MethodInvokerRegistration* TypeRegistry::FindMethodInvoker(
    MemberId member) const noexcept {
    for (const MethodInvokerRegistration& registration : methodInvokers_) {
        if (registration.member == member) return &registration;
    }
    return nullptr;
}

Base::Result<void> TypeRegistry::Freeze() noexcept {
    if (frozen_) return {};

    for (const TypeInfo& type : types_) {
        if (type.BaseType() != InvalidTypeId &&
            FindType(type.BaseType()) == nullptr) {
            return MissingRelatedTypeStatus();
        }
        for (const PropertyInfo& property : type.Properties()) {
            if (FindType(property.ValueType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
            const PropertyAccessorRegistration* accessor =
                FindPropertyAccessor(property.Id());
            if (accessor != nullptr && !IsValidPropertyBehavior(
                    accessor->access,
                    accessor->get,
                    accessor->set,
                    accessor->provider,
                    accessor->context)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Property access metadata is invalid");
            }
        }
        for (const EventInfo& eventInfo : type.Events()) {
            if (FindType(eventInfo.EventArgsType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }
        for (const MethodInfo& method : type.Methods()) {
            const MethodInvokerRegistration* invoker =
                FindMethodInvoker(method.Id());
            if (invoker == nullptr || invoker->invoke == nullptr ||
                (method.ReturnType() != InvalidTypeId &&
                 FindType(method.ReturnType()) == nullptr)) {
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

bool TypeRegistry::IsInstanceOf(
    const Base::Object& object,
    TypeId expectedType) const noexcept {
    return IsDerivedFrom(object.RuntimeType(), expectedType);
}

Base::Result<void> TypeRegistry::BuildSnapshot(
    Base::String& output) const noexcept {
    if (!frozen_) return SnapshotBeforeFreezeStatus();

    Base::String snapshot(&output.Allocator());
    Base::Result<void> result = Append(
        snapshot, Base::StringView("AERO-TYPE-REGISTRY|2\n"));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint32_t> typeOrder;
    result = BuildOrder(types_.Size(), typeOrder,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return types_[left].Id() < types_[right].Id();
        });
    if (!result) return result.GetStatus();

    for (std::uint32_t typeIndex : typeOrder) {
        const TypeInfo& type = types_[typeIndex];
        result = AppendTypeLine(snapshot, type);
        if (!result) return result.GetStatus();

        Base::Vector<std::uint32_t> propertyOrder;
        const Base::Span<const PropertyInfo> properties = type.Properties();
        result = BuildOrder(properties.Size(), propertyOrder,
            [&properties](std::uint32_t left, std::uint32_t right) noexcept {
                return properties[left].Id() < properties[right].Id();
            });
        if (!result) return result.GetStatus();
        for (std::uint32_t propertyIndex : propertyOrder) {
            result = AppendPropertyLine(
                snapshot, *this, properties[propertyIndex]);
            if (!result) return result.GetStatus();
        }

        Base::Vector<std::uint32_t> eventOrder;
        const Base::Span<const EventInfo> events = type.Events();
        result = BuildOrder(events.Size(), eventOrder,
            [&events](std::uint32_t left, std::uint32_t right) noexcept {
                return events[left].Id() < events[right].Id();
            });
        if (!result) return result.GetStatus();
        for (std::uint32_t eventIndex : eventOrder) {
            result = AppendEventLine(snapshot, events[eventIndex]);
            if (!result) return result.GetStatus();
        }

        Base::Vector<std::uint32_t> methodOrder;
        const Base::Span<const MethodInfo> methods = type.Methods();
        result = BuildOrder(methods.Size(), methodOrder,
            [&methods](std::uint32_t left, std::uint32_t right) noexcept {
                return methods[left].Id() < methods[right].Id();
            });
        if (!result) return result.GetStatus();
        for (std::uint32_t methodIndex : methodOrder) {
            result = AppendMethodLine(snapshot, methods[methodIndex]);
            if (!result) return result.GetStatus();
        }
    }

    output = std::move(snapshot);
    return {};
}

Base::Result<Base::HashCode> TypeRegistry::ComputeSnapshotHash() const noexcept {
    Base::String snapshot;
    Base::Result<void> result = BuildSnapshot(snapshot);
    if (!result) return result.GetStatus();
    return Base::HashBytes(
        snapshot.View().Data(),
        snapshot.View().SizeBytes(),
        UINT64_C(0xA3E0C5D2B1749F61));
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

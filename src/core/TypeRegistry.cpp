#include <Aero/Core/TypeRegistry.hpp>

#include <Aero/Base/Assert.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace Aero::Core {
namespace {

constexpr Base::HashCode StableOffsetBasis =
    UINT64_C(14695981039346656037);
constexpr Base::HashCode StablePrime = UINT64_C(1099511628211);
constexpr Base::HashCode NonZeroFallback = UINT64_C(0x9E3779B97F4A7C15);

class StableIdBuilder final {
public:
    void AddByte(std::uint8_t value) noexcept {
        value_ ^= static_cast<Base::HashCode>(value);
        value_ *= StablePrime;
    }

    void AddBytes(const void* data, std::uint32_t size) noexcept {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (std::uint32_t index = 0U; index < size; ++index) {
            AddByte(bytes[index]);
        }
    }

    void AddString(Base::StringView value) noexcept {
        AddU32(value.SizeBytes());
        AddBytes(value.Data(), value.SizeBytes());
    }

    void AddU32(std::uint32_t value) noexcept {
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    void AddU64(std::uint64_t value) noexcept {
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    AERO_NODISCARD std::uint64_t Finish() const noexcept {
        const std::uint64_t result = Base::MixHash64(value_);
        return result != 0U ? result : NonZeroFallback;
    }

private:
    Base::HashCode value_ = StableOffsetBasis;
};

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

AERO_NODISCARD Base::Result<void> Append(
    Base::String& output, Base::StringView text) noexcept {
    return output.TryAppendUnchecked(text);
}

AERO_NODISCARD Base::Result<void> AppendHex(
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

AERO_NODISCARD Base::Result<void> AppendSizedText(
    Base::String& output,
    Base::StringView text) noexcept {
    Base::Result<void> result = AppendHex(
        output, static_cast<std::uint64_t>(text.SizeBytes()), 8U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView(":"));
    if (!result) {
        return result.GetStatus();
    }
    return Append(output, text);
}

template<class Less>
AERO_NODISCARD Base::Result<void> BuildOrder(
    std::uint32_t count,
    Base::Vector<std::uint32_t>& order,
    Less less) noexcept {
    Base::Result<void> result = order.TryReserve(count);
    if (!result) {
        return result.GetStatus();
    }

    for (std::uint32_t index = 0U; index < count; ++index) {
        result = order.TryPushBack(index);
        if (!result) {
            return result.GetStatus();
        }
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

AERO_NODISCARD Base::Result<void> AppendTypeLine(
    Base::String& output,
    const TypeInfo& type) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("T|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, type.Id(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, type.BaseType(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(
        output, static_cast<std::uint32_t>(type.Flags()), 8U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendSizedText(output, type.XamlNamespace());
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendSizedText(output, type.Name());
    if (!result) {
        return result.GetStatus();
    }
    return Append(output, Base::StringView("\n"));
}

AERO_NODISCARD Base::Result<void> AppendPropertyLine(
    Base::String& output,
    const PropertyInfo& property) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("P|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, property.Id(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, property.OwnerType(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, property.ValueType(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(
        output, static_cast<std::uint32_t>(property.Flags()), 8U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendSizedText(output, property.Name());
    if (!result) {
        return result.GetStatus();
    }
    return Append(output, Base::StringView("\n"));
}

AERO_NODISCARD Base::Result<void> AppendEventLine(
    Base::String& output,
    const EventInfo& eventInfo) noexcept {
    Base::Result<void> result = Append(output, Base::StringView("E|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, eventInfo.Id(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, eventInfo.OwnerType(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(output, eventInfo.EventArgsType(), 16U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendHex(
        output, static_cast<std::uint32_t>(eventInfo.Flags()), 8U);
    if (!result) {
        return result.GetStatus();
    }
    result = Append(output, Base::StringView("|"));
    if (!result) {
        return result.GetStatus();
    }
    result = AppendSizedText(output, eventInfo.Name());
    if (!result) {
        return result.GetStatus();
    }
    return Append(output, Base::StringView("\n"));
}

} // namespace

TypeId MakeTypeId(
    Base::StringView xamlNamespace,
    Base::StringView name) noexcept {
    static constexpr char Domain[] = "AERO.TYPE.V1";
    StableIdBuilder builder;
    builder.AddBytes(
        Domain, static_cast<std::uint32_t>(sizeof(Domain) - 1U));
    builder.AddString(xamlNamespace);
    builder.AddString(name);
    return builder.Finish();
}

MemberId MakeMemberId(
    TypeId ownerType,
    MemberKind kind,
    Base::StringView name) noexcept {
    static constexpr char Domain[] = "AERO.MEMBER.V1";
    StableIdBuilder builder;
    builder.AddBytes(
        Domain, static_cast<std::uint32_t>(sizeof(Domain) - 1U));
    builder.AddU64(ownerType);
    builder.AddByte(static_cast<std::uint8_t>(kind));
    builder.AddString(name);
    return builder.Finish();
}

TypeRegistry::TypeRegistry(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_),
      typeIndex_(allocator_),
      memberIndex_(allocator_) {}

Base::Result<TypeId> TypeRegistry::TryRegisterType(
    const TypeRegistration& registration) noexcept {
    if (frozen_) {
        return RegistryFrozenStatus();
    }
    if (registration.xamlNamespace.Empty() || registration.name.Empty()) {
        return EmptyTypeNameStatus();
    }

    const TypeId id = MakeTypeId(
        registration.xamlNamespace, registration.name);
    const std::uint32_t* existingIndex = typeIndex_.Find(id);
    if (existingIndex != nullptr) {
        const TypeInfo& existing = types_[*existingIndex];
        if (existing.XamlNamespace() == registration.xamlNamespace &&
            existing.Name() == registration.name) {
            return DuplicateTypeStatus();
        }
        return IdCollisionStatus();
    }

    TypeInfo info(allocator_);
    info.id_ = id;
    info.baseType_ = registration.baseType;
    info.flags_ = registration.flags;
    info.factory_ = registration.factory;

    Base::Result<void> result =
        info.xamlNamespace_.TryAssign(registration.xamlNamespace);
    if (!result) {
        return result.GetStatus();
    }
    result = info.name_.TryAssign(registration.name);
    if (!result) {
        return result.GetStatus();
    }

    const std::uint32_t index = types_.Size();
    result = types_.TryPushBack(std::move(info));
    if (!result) {
        return result.GetStatus();
    }

    Base::Result<Base::HashMap<TypeId, std::uint32_t>::InsertResult> inserted =
        typeIndex_.TryInsert(id, index);
    if (!inserted) {
        types_.PopBack();
        return inserted.GetStatus();
    }
    if (!inserted.Value().inserted) {
        types_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterProperty(
    TypeId ownerType,
    const PropertyRegistration& registration) noexcept {
    if (frozen_) {
        return RegistryFrozenStatus();
    }
    if (registration.name.Empty()) {
        return EmptyMemberNameStatus();
    }
    if (registration.valueType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Property value type must be valid");
    }

    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) {
        return MissingOwnerStatus();
    }

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::Property, registration.name);
    const MemberLocation* existingLocation = memberIndex_.Find(id);
    if (existingLocation != nullptr) {
        const PropertyInfo* existing = PropertyAt(*existingLocation);
        if (existing != nullptr &&
            existing->OwnerType() == ownerType &&
            existing->Name() == registration.name) {
            return DuplicateMemberStatus();
        }
        return IdCollisionStatus();
    }

    TypeInfo& owner = types_[*ownerIndex];
    PropertyInfo property(allocator_);
    property.id_ = id;
    property.ownerType_ = ownerType;
    property.valueType_ = registration.valueType;
    property.flags_ = registration.flags;
    Base::Result<void> result = property.name_.TryAssign(registration.name);
    if (!result) {
        return result.GetStatus();
    }

    const std::uint32_t propertyIndex = owner.properties_.Size();
    result = owner.properties_.TryPushBack(std::move(property));
    if (!result) {
        return result.GetStatus();
    }

    const MemberLocation location{
        *ownerIndex, propertyIndex, MemberKind::Property};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(id, location);
    if (!inserted) {
        owner.properties_.PopBack();
        return inserted.GetStatus();
    }
    if (!inserted.Value().inserted) {
        owner.properties_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<MemberId> TypeRegistry::TryRegisterEvent(
    TypeId ownerType,
    const EventRegistration& registration) noexcept {
    if (frozen_) {
        return RegistryFrozenStatus();
    }
    if (registration.name.Empty()) {
        return EmptyMemberNameStatus();
    }
    if (registration.eventArgsType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Event argument type must be valid");
    }

    std::uint32_t* ownerIndex = typeIndex_.Find(ownerType);
    if (ownerIndex == nullptr) {
        return MissingOwnerStatus();
    }

    const MemberId id = MakeMemberId(
        ownerType, MemberKind::Event, registration.name);
    const MemberLocation* existingLocation = memberIndex_.Find(id);
    if (existingLocation != nullptr) {
        const EventInfo* existing = EventAt(*existingLocation);
        if (existing != nullptr &&
            existing->OwnerType() == ownerType &&
            existing->Name() == registration.name) {
            return DuplicateMemberStatus();
        }
        return IdCollisionStatus();
    }

    TypeInfo& owner = types_[*ownerIndex];
    EventInfo eventInfo(allocator_);
    eventInfo.id_ = id;
    eventInfo.ownerType_ = ownerType;
    eventInfo.eventArgsType_ = registration.eventArgsType;
    eventInfo.flags_ = registration.flags;
    Base::Result<void> result = eventInfo.name_.TryAssign(registration.name);
    if (!result) {
        return result.GetStatus();
    }

    const std::uint32_t eventIndex = owner.events_.Size();
    result = owner.events_.TryPushBack(std::move(eventInfo));
    if (!result) {
        return result.GetStatus();
    }

    const MemberLocation location{
        *ownerIndex, eventIndex, MemberKind::Event};
    Base::Result<Base::HashMap<MemberId, MemberLocation>::InsertResult> inserted =
        memberIndex_.TryInsert(id, location);
    if (!inserted) {
        owner.events_.PopBack();
        return inserted.GetStatus();
    }
    if (!inserted.Value().inserted) {
        owner.events_.PopBack();
        return IdCollisionStatus();
    }
    return id;
}

Base::Result<void> TypeRegistry::Freeze() noexcept {
    if (frozen_) {
        return {};
    }

    for (const TypeInfo& type : types_) {
        if (type.BaseType() != InvalidTypeId &&
            FindType(type.BaseType()) == nullptr) {
            return MissingRelatedTypeStatus();
        }
        for (const PropertyInfo& property : type.Properties()) {
            if (FindType(property.ValueType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }
        for (const EventInfo& eventInfo : type.Events()) {
            if (FindType(eventInfo.EventArgsType()) == nullptr) {
                return MissingRelatedTypeStatus();
            }
        }
    }

    Base::Vector<std::uint8_t> state(allocator_);
    Base::Result<void> result = state.TryResize(types_.Size(), std::uint8_t{0U});
    if (!result) {
        return result.GetStatus();
    }

    Base::Vector<std::uint32_t> path(allocator_);
    result = path.TryReserve(types_.Size());
    if (!result) {
        return result.GetStatus();
    }

    for (std::uint32_t start = 0U; start < types_.Size(); ++start) {
        if (state[start] == 2U) {
            continue;
        }

        path.Clear();
        std::uint32_t current = start;
        while (state[current] != 2U) {
            if (state[current] == 1U) {
                return InheritanceCycleStatus();
            }

            state[current] = 1U;
            result = path.TryPushBack(current);
            if (!result) {
                return result.GetStatus();
            }

            const TypeId baseType = types_[current].BaseType();
            if (baseType == InvalidTypeId) {
                break;
            }

            const std::uint32_t* baseIndex = typeIndex_.Find(baseType);
            AERO_ASSERT(baseIndex != nullptr);
            current = *baseIndex;
        }

        for (std::uint32_t index : path) {
            state[index] = 2U;
        }
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
    if (type == nullptr ||
        type->XamlNamespace() != xamlNamespace ||
        type->Name() != name) {
        return nullptr;
    }
    return type;
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
         current != InvalidTypeId && depth <= types_.Size();
         ++depth) {
        const PropertyInfo* property = FindProperty(
            MakeMemberId(current, MemberKind::Property, name));
        if (property != nullptr &&
            property->OwnerType() == current &&
            property->Name() == name) {
            return property;
        }
        if (!includeBaseTypes) {
            return nullptr;
        }
        const TypeInfo* type = FindType(current);
        if (type == nullptr) {
            return nullptr;
        }
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
         current != InvalidTypeId && depth <= types_.Size();
         ++depth) {
        const EventInfo* eventInfo = FindEvent(
            MakeMemberId(current, MemberKind::Event, name));
        if (eventInfo != nullptr &&
            eventInfo->OwnerType() == current &&
            eventInfo->Name() == name) {
            return eventInfo;
        }
        if (!includeBaseTypes) {
            return nullptr;
        }
        const TypeInfo* type = FindType(current);
        if (type == nullptr) {
            return nullptr;
        }
        current = type->BaseType();
    }
    return nullptr;
}

bool TypeRegistry::IsDerivedFrom(
    TypeId type,
    TypeId expectedBase) const noexcept {
    if (type == InvalidTypeId || expectedBase == InvalidTypeId) {
        return false;
    }

    TypeId current = type;
    for (std::uint32_t depth = 0U;
         current != InvalidTypeId && depth <= types_.Size();
         ++depth) {
        if (current == expectedBase) {
            return true;
        }
        const TypeInfo* info = FindType(current);
        if (info == nullptr) {
            return false;
        }
        current = info->BaseType();
    }
    return false;
}

Base::Result<void> TypeRegistry::BuildSnapshot(
    Base::String& output) const noexcept {
    if (!frozen_) {
        return SnapshotBeforeFreezeStatus();
    }

    Base::String snapshot(&output.Allocator());
    Base::Result<void> result = Append(
        snapshot, Base::StringView("AERO-TYPE-REGISTRY|1\n"));
    if (!result) {
        return result.GetStatus();
    }

    Base::Vector<std::uint32_t> typeOrder(allocator_);
    result = BuildOrder(
        types_.Size(),
        typeOrder,
        [this](std::uint32_t left, std::uint32_t right) noexcept {
            return types_[left].Id() < types_[right].Id();
        });
    if (!result) {
        return result.GetStatus();
    }

    for (std::uint32_t typeIndex : typeOrder) {
        const TypeInfo& type = types_[typeIndex];
        result = AppendTypeLine(snapshot, type);
        if (!result) {
            return result.GetStatus();
        }

        Base::Vector<std::uint32_t> propertyOrder(allocator_);
        const Base::Span<const PropertyInfo> properties = type.Properties();
        result = BuildOrder(
            properties.Size(),
            propertyOrder,
            [&properties](std::uint32_t left, std::uint32_t right) noexcept {
                return properties[left].Id() < properties[right].Id();
            });
        if (!result) {
            return result.GetStatus();
        }
        for (std::uint32_t propertyIndex : propertyOrder) {
            result = AppendPropertyLine(snapshot, properties[propertyIndex]);
            if (!result) {
                return result.GetStatus();
            }
        }

        Base::Vector<std::uint32_t> eventOrder(allocator_);
        const Base::Span<const EventInfo> events = type.Events();
        result = BuildOrder(
            events.Size(),
            eventOrder,
            [&events](std::uint32_t left, std::uint32_t right) noexcept {
                return events[left].Id() < events[right].Id();
            });
        if (!result) {
            return result.GetStatus();
        }
        for (std::uint32_t eventIndex : eventOrder) {
            result = AppendEventLine(snapshot, events[eventIndex]);
            if (!result) {
                return result.GetStatus();
            }
        }
    }

    output = std::move(snapshot);
    return {};
}

Base::Result<Base::HashCode> TypeRegistry::ComputeSnapshotHash() const noexcept {
    Base::String snapshot(allocator_);
    Base::Result<void> result = BuildSnapshot(snapshot);
    if (!result) {
        return result.GetStatus();
    }
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
    if (location.kind != MemberKind::Property) {
        return nullptr;
    }
    const TypeInfo* owner = TypeAt(location.typeIndex);
    if (owner == nullptr || location.memberIndex >= owner->properties_.Size()) {
        return nullptr;
    }
    return &owner->properties_[location.memberIndex];
}

const EventInfo* TypeRegistry::EventAt(
    const MemberLocation& location) const noexcept {
    if (location.kind != MemberKind::Event) {
        return nullptr;
    }
    const TypeInfo* owner = TypeAt(location.typeIndex);
    if (owner == nullptr || location.memberIndex >= owner->events_.Size()) {
        return nullptr;
    }
    return &owner->events_[location.memberIndex];
}

} // namespace Aero::Core

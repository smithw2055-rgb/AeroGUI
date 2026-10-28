#include "gui/core/TypeRegistryCore.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlSchemaConventions.hpp"
#include "gui/markup/XamlBinaryCodec.hpp"
#include "gui/core/ValueConversion.hpp"

// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.

#include <Aero/Base/Assert.hpp>

#include <new>

namespace Aero::Markup {

enum class ManifestMemberKind : std::uint8_t {
    Property = 0U,
    Event
};

namespace {

constexpr std::uint32_t ManifestMagic = UINT32_C(0x48435341); // ASCH
constexpr std::uint32_t ManifestEncodingVersion = 1U;

Base::Status InvalidManifest(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Base::Status ManifestNotReady() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML schema manifest is not initialized");
}

Base::Status TypeNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest type was not found");
}

Base::Status MemberNotFound() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML schema manifest member was not found");
}



} // namespace

struct SchemaManifest::State {
    struct TypeRecord {
        explicit TypeRecord(Base::IAllocator& allocator) noexcept
            : xamlNamespace(&allocator), name(&allocator) {}

        Meta::TypeId id = Meta::InvalidTypeId;
        Meta::TypeId baseType = Meta::InvalidTypeId;
        Meta::MetadataTypeKind kind = Meta::MetadataTypeKind::Object;
        Meta::TypeFlags flags = Meta::TypeFlags::None;
        Meta::MemberId contentMember = Meta::InvalidMemberId;
        Base::String xamlNamespace;
        Base::String name;
    };

    struct MemberRecord {
        explicit MemberRecord(Base::IAllocator& allocator) noexcept
            : name(&allocator) {}

        Meta::MemberId id = Meta::InvalidMemberId;
        ManifestMemberKind kind = ManifestMemberKind::Property;
        Meta::TypeId ownerType = Meta::InvalidTypeId;
        Meta::TypeId valueType = Meta::InvalidTypeId;
        std::uint32_t flags = 0U;
        Base::String name;
    };

    explicit State(Base::IAllocator& allocator) noexcept
        : types(&allocator),
          members(&allocator),
          typeIndex(&allocator),
          memberIndex(&allocator) {}

    CompiledCacheIdentity identity;
    Base::Vector<TypeRecord> types;
    Base::Vector<MemberRecord> members;
    Base::HashMap<Meta::TypeId, std::uint32_t> typeIndex;
    Base::HashMap<Meta::MemberId, std::uint32_t> memberIndex;
    bool valid = false;

    Base::Result<void> RebuildIndexes() noexcept {
        typeIndex.Clear();
        memberIndex.Clear();
        for (std::uint32_t index = 0U; index < types.Size(); ++index) {
            typename Base::HashMap<Meta::TypeId, std::uint32_t>::InsertResult inserted = typeIndex.Insert(types[index].id, index);
            if (!inserted.inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            typename Base::HashMap<Meta::MemberId, std::uint32_t>::InsertResult inserted = memberIndex.Insert(members[index].id, index);
            if (!inserted.inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate MemberId values");
            }
        }
        return {};
    }

    const TypeRecord* FindType(Meta::TypeId id) const noexcept {
        const std::uint32_t* index = typeIndex.Find(id);
        return index != nullptr && *index < types.Size()
            ? &types[*index] : nullptr;
    }

    const TypeRecord* FindType(
        Base::StringView xamlNamespace,
        Base::StringView name) const noexcept {
        for (const TypeRecord& type : types) {
            if (type.xamlNamespace.View() == xamlNamespace &&
                type.name.View() == name) {
                return &type;
            }
        }
        return nullptr;
    }

    const MemberRecord* FindMember(Meta::MemberId id) const noexcept {
        const std::uint32_t* index = memberIndex.Find(id);
        return index != nullptr && *index < members.Size()
            ? &members[*index] : nullptr;
    }

    const MemberRecord* FindMember(
        Meta::TypeId ownerType,
        Base::StringView name,
        ManifestMemberKind kind,
        bool includeBaseTypes) const noexcept {
        Meta::TypeId current = ownerType;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            for (const MemberRecord& member : members) {
                if (member.ownerType == current &&
                    member.kind == kind &&
                    member.name.View() == name) {
                    return &member;
                }
            }
            if (!includeBaseTypes) break;
            const TypeRecord* type = FindType(current);
            if (type == nullptr) break;
            current = type->baseType;
        }
        return nullptr;
    }

    bool IsDerivedFrom(
        Meta::TypeId type,
        Meta::TypeId expectedBase) const noexcept {
        if (type == Meta::InvalidTypeId ||
            expectedBase == Meta::InvalidTypeId) {
            return false;
        }
        Meta::TypeId current = type;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            if (current == expectedBase) return true;
            const TypeRecord* descriptor = FindType(current);
            if (descriptor == nullptr) return false;
            current = descriptor->baseType;
        }
        return false;
    }

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
        Meta::TypeId targetType,
        Meta::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept {
        const MemberRecord* property = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Property,
            true);
        if (property != nullptr &&
            syntax == MemberSyntax::Attribute &&
            HasPropertyFlag(
                static_cast<Meta::PropertyFlags>(property->flags),
                Meta::PropertyFlags::Collection)) {
            char aliasBuffer[128];
            if (memberName.SizeBytes() + 4U < sizeof(aliasBuffer)) {
                std::memcpy(aliasBuffer, memberName.Data(), memberName.SizeBytes());
                std::memcpy(aliasBuffer + memberName.SizeBytes(), "Text", 4U);
                const MemberRecord* alias = FindMember(
                    ownerType,
                    Base::StringView(aliasBuffer, memberName.SizeBytes() + 4U),
                    ManifestMemberKind::Property,
                    true);
                if (alias != nullptr &&
                    !HasPropertyFlag(
                        static_cast<Meta::PropertyFlags>(alias->flags),
                        Meta::PropertyFlags::Collection)) {
                    property = alias;
                }
            }
        }
        if (property != nullptr) {
            const Meta::PropertyFlags flags =
                static_cast<Meta::PropertyFlags>(property->flags);
            const bool attached = HasPropertyFlag(
                flags, Meta::PropertyFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached property");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, property->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML property element owner is incompatible with the target type");
            }
            return MakeResolvedPropertyMember(
                property->id,
                property->ownerType,
                property->valueType,
                flags);
        }

        const MemberRecord* event = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Event,
            true);
        if (event != nullptr) {
            const Meta::EventFlags flags =
                static_cast<Meta::EventFlags>(event->flags);
            const bool attached = HasEventFlag(
                flags, Meta::EventFlags::Attached);
            const bool routed = HasEventFlag(
                flags, Meta::EventFlags::Routed);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute &&
                !attached && !routed) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Explicit XAML attribute owner requires an attached event");
            }
            if (ownerWasExplicit &&
                syntax == MemberSyntax::PropertyElement && !attached &&
                !IsDerivedFrom(targetType, event->ownerType)) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "XAML event element owner is incompatible with the target type");
            }
            ResolvedMember resolved = MakeResolvedEventMember(
                event->id,
                event->ownerType,
                event->valueType,
                flags);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && routed) {
                resolved.attached = true;
            }
            return resolved;
        }
        return MemberNotFound();
    }
};

static_assert(
    sizeof(SchemaManifest::State) <= 2048,
    "SchemaManifest inline state storage is too small");
static_assert(
    alignof(SchemaManifest::State) <= alignof(std::max_align_t),
    "SchemaManifest inline state alignment is insufficient");

namespace {

template<class T>
Base::Result<T*> AllocateObject(
    Base::IAllocator& allocator) noexcept {
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML schema manifest allocation failed");
    }
    return new (memory) T(allocator);
}

void DestroyManifestState(
    Base::IAllocator& allocator,
    SchemaManifest::State*& state) noexcept {
    if (state == nullptr) return;
    state->~State();
    allocator.Deallocate(
        state,
        sizeof(SchemaManifest::State),
        alignof(SchemaManifest::State),
        Base::MemoryTag::Markup);
    state = nullptr;
}

Base::Result<void> AppendIdentity(
    Base::Vector<std::uint8_t>& output,
    const CompiledCacheIdentity& identity) noexcept {
    Base::Result<void> result = AppendU32(
        output, identity.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataProgramFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.schemaVersion);
    if (!result) return result.GetStatus();
    return AppendU64(output, identity.metadataSchemaHash);
}

Base::Result<CompiledCacheIdentity> ReadIdentity(
    Decoder& decoder) noexcept {
    CompiledCacheIdentity identity;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataProgramFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

} // namespace

SchemaManifest::SchemaManifest(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator : &Base::GetDefaultAllocator()) {}

SchemaManifest::SchemaManifest(
    Base::IAllocator& allocator,
    SchemaManifest::State* state) noexcept
    : allocator_(&allocator),
      state_(state) {}

SchemaManifest::~SchemaManifest() noexcept {
    if (state_ == nullptr) return;
    DestroyManifestState(*allocator_, state_);
    state_ = nullptr;
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = &Base::GetDefaultAllocator();
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        DestroyManifestState(*allocator_, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = &Base::GetDefaultAllocator();
    return *this;
}

Base::Result<SchemaManifest> SchemaManifest::Capture(
    const Schema& schema,
    Base::IAllocator* allocator) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML schema manifest capture requires a frozen schema");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<SchemaManifest::State*> created = AllocateObject<SchemaManifest::State>(selected);
    if (!created) return created.GetStatus();
    SchemaManifest::State* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Meta::TypeRegistry& descriptors = schema.Types();
    impl->types.Reserve(descriptors.TypeCount());
    impl->members.Reserve(
        descriptors.PropertyCount() + descriptors.EventCount());

    for (const Meta::TypeInfo& type : descriptors.Types()) {
        SchemaManifest::State::TypeRecord record(selected);
        record.id = type.Id();
        record.baseType = type.BaseType();
        record.kind = type.Kind();
        record.flags = type.Flags();
        Base::Result<void> assigned = record.xamlNamespace.Assign(
            type.XamlNamespace());
        if (assigned) assigned = record.name.Assign(type.Name());
        if (!assigned) {
            DestroyManifestState(selected, impl);
            return assigned.GetStatus();
        }
        Base::Result<ResolvedMember> content =
            schema.ResolveContentMember(type.Id());
        if (content) {
            record.contentMember = content.Value().id;
        } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        impl->types.PushBack(
            std::move(record));

        for (const Meta::PropertyInfo& property : type.Properties()) {
            SchemaManifest::State::MemberRecord member(selected);
            member.id = property.Id();
            member.kind = ManifestMemberKind::Property;
            member.ownerType = property.OwnerType();
            member.valueType = property.ValueType();
            member.flags = static_cast<std::uint32_t>(property.Flags());
            assigned = member.name.Assign(property.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            impl->members.PushBack(std::move(member));
        }

        for (const Meta::EventInfo& event : type.Events()) {
            SchemaManifest::State::MemberRecord member(selected);
            member.id = event.Id();
            member.kind = ManifestMemberKind::Event;
            member.ownerType = event.OwnerType();
            member.valueType = event.EventArgsType();
            member.flags = static_cast<std::uint32_t>(event.Flags());
            assigned = member.name.Assign(event.Name());
            if (!assigned) {
                DestroyManifestState(selected, impl);
                return assigned.GetStatus();
            }
            impl->members.PushBack(std::move(member));
        }
    }

    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<SchemaManifest> SchemaManifest::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const SchemaManifestLimits& limits,
    Base::IAllocator* allocator) noexcept {
    if (limits.maxTypes == 0U ||
        limits.maxMembers == 0U ||
        limits.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest limits must be positive");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Decoder decoder(bytes, "XAML schema manifest payload is truncated");
    Base::Result<std::uint32_t> magic = decoder.ReadU32();
    if (!magic) return magic.GetStatus();
    Base::Result<std::uint32_t> encoding = decoder.ReadU32();
    if (!encoding) return encoding.GetStatus();
    Base::Result<std::uint32_t> format = decoder.ReadU32();
    if (!format) return format.GetStatus();
    if (magic.Value() != ManifestMagic ||
        encoding.Value() != ManifestEncodingVersion ||
        format.Value() != XamlSchemaManifestFormatVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest format is not supported");
    }

    Base::Result<SchemaManifest::State*> created = AllocateObject<SchemaManifest::State>(selected);
    if (!created) return created.GetStatus();
    SchemaManifest::State* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity = ReadIdentity(decoder);
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    CompiledCacheIdentity current;
    current.metadataSchemaHash = identity.Value().metadataSchemaHash;
    if (CompareCompiledCacheIdentity(identity.Value(), current) !=
        CompiledCacheCompatibility::Compatible) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest ABI is incompatible with this tool");
    }
    impl->identity = identity.Value();

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) {
        DestroyManifestState(selected, impl);
        return typeCount.GetStatus();
    }
    Base::Result<std::uint32_t> memberCount = decoder.ReadU32();
    if (!memberCount) {
        DestroyManifestState(selected, impl);
        return memberCount.GetStatus();
    }
    if (typeCount.Value() > limits.maxTypes ||
        memberCount.Value() > limits.maxMembers) {
        DestroyManifestState(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest descriptor count exceeds limits");
    }
    impl->types.Reserve(typeCount.Value());
    impl->members.Reserve(memberCount.Value());

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        SchemaManifest::State::TypeRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint64_t> baseType = decoder.ReadU64();
        if (!baseType) {
            DestroyManifestState(selected, impl);
            return baseType.GetStatus();
        }
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<std::uint64_t> content = decoder.ReadU64();
        if (!content) {
            DestroyManifestState(selected, impl);
            return content.GetStatus();
        }
        Base::Result<Base::String> xamlNamespace = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!xamlNamespace) {
            DestroyManifestState(selected, impl);
            return xamlNamespace.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        if (id.Value() == Meta::InvalidTypeId || name.Value().Empty() ||
            Meta::MakeTypeId(
                xamlNamespace.Value().View(),
                name.Value().View()) != id.Value() ||
            kind.Value() > static_cast<std::uint32_t>(Meta::MetadataTypeKind::Primitive)) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type descriptor is invalid");
        }
        record.id = id.Value();
        record.baseType = baseType.Value();
        record.kind = static_cast<Meta::MetadataTypeKind>(kind.Value());
        record.flags = static_cast<Meta::TypeFlags>(flags.Value());
        record.contentMember = content.Value();
        record.xamlNamespace = std::move(xamlNamespace).Value();
        record.name = std::move(name).Value();
        impl->types.PushBack(std::move(record));
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        SchemaManifest::State::MemberRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyManifestState(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) {
            DestroyManifestState(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> reservedField = decoder.ReadU32();
        if (!reservedField) {
            DestroyManifestState(selected, impl);
            return reservedField.GetStatus();
        }
        Base::Result<std::uint64_t> owner = decoder.ReadU64();
        if (!owner) {
            DestroyManifestState(selected, impl);
            return owner.GetStatus();
        }
        Base::Result<std::uint64_t> valueType = decoder.ReadU64();
        if (!valueType) {
            DestroyManifestState(selected, impl);
            return valueType.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyManifestState(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyManifestState(selected, impl);
            return name.GetStatus();
        }
        const bool validKind =
            kind.Value() <= static_cast<std::uint8_t>(ManifestMemberKind::Event);
        const Meta::MemberKind metadataKind =
            kind.Value() == static_cast<std::uint8_t>(ManifestMemberKind::Event)
            ? Meta::MemberKind::Event
            : Meta::MemberKind::Property;
        if (id.Value() == Meta::InvalidMemberId ||
            owner.Value() == Meta::InvalidTypeId ||
            valueType.Value() == Meta::InvalidTypeId ||
            name.Value().Empty() ||
            reservedField.Value() != 0U ||
            !validKind ||
            Meta::MakeMemberId(
                owner.Value(), metadataKind, name.Value().View()) != id.Value()) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member descriptor is invalid");
        }
        record.id = id.Value();
        record.kind = static_cast<ManifestMemberKind>(kind.Value());
        record.ownerType = owner.Value();
        record.valueType = valueType.Value();
        record.flags = flags.Value();
        record.name = std::move(name).Value();
        impl->members.PushBack(std::move(record));
    }

    if (!decoder.AtEnd()) {
        DestroyManifestState(selected, impl);
        return InvalidManifest("XAML schema manifest has trailing bytes");
    }
    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyManifestState(selected, impl);
        return indexed.GetStatus();
    }
    for (const SchemaManifest::State::TypeRecord& type : impl->types) {
        if (type.baseType != Meta::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Meta::TypeId ancestorId = type.id;
        std::uint32_t depth = 0U;
        while (ancestorId != Meta::InvalidTypeId && depth <= impl->types.Size()) {
            const SchemaManifest::State::TypeRecord* currentType = impl->FindType(ancestorId);
            if (currentType == nullptr) break;
            ancestorId = currentType->baseType;
            ++depth;
        }
        if (ancestorId != Meta::InvalidTypeId) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Meta::InvalidMemberId) {
            const SchemaManifest::State::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyManifestState(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const SchemaManifest::State::MemberRecord& member : impl->members) {
        if (impl->FindType(member.ownerType) == nullptr ||
            impl->FindType(member.valueType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest member type is missing");
        }
    }
    impl->valid = true;
    return SchemaManifest(selected, impl);
}

Base::Result<Base::Vector<std::uint8_t>>
SchemaManifest::Serialize() const noexcept {
    if (!IsValid()) return ManifestNotReady();
    Base::Vector<std::uint8_t> output(allocator_);
    Base::Result<void> result = AppendU32(output, ManifestMagic);
    if (result) result = AppendU32(output, ManifestEncodingVersion);
    if (result) result = AppendU32(output, XamlSchemaManifestFormatVersion);
    if (result) result = AppendIdentity(output, state_->identity);
    if (result) result = AppendU32(output, state_->types.Size());
    if (result) result = AppendU32(output, state_->members.Size());
    if (!result) return result.GetStatus();

    for (const SchemaManifest::State::TypeRecord& type : state_->types) {
        result = AppendU64(output, type.id);
        if (result) result = AppendU64(output, type.baseType);
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.kind));
        if (result) result = AppendU32(
            output, static_cast<std::uint32_t>(type.flags));
        if (result) result = AppendU64(output, type.contentMember);
        if (result) result = AppendString(output, type.xamlNamespace.View());
        if (result) result = AppendString(output, type.name.View());
        if (!result) return result.GetStatus();
    }
    for (const SchemaManifest::State::MemberRecord& member : state_->members) {
        result = AppendU64(output, member.id);
        if (result) result = AppendU8(
            output, static_cast<std::uint8_t>(member.kind));
        if (result) result = AppendU32(output, 0U);
        if (result) result = AppendU64(output, member.ownerType);
        if (result) result = AppendU64(output, member.valueType);
        if (result) result = AppendU32(output, member.flags);
        if (result) result = AppendString(output, member.name.View());
        if (!result) return result.GetStatus();
    }
    return output;
}

bool SchemaManifest::IsValid() const noexcept {
    return state_ != nullptr && state_->valid;
}

std::uint32_t SchemaManifest::TypeCount() const noexcept {
    return IsValid() ? state_->types.Size() : 0U;
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    return IsValid() ? state_->members.Size() : 0U;
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return state_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* type = state_->FindType(
        IsSystemNamespace(xamlNamespace)
            ? Meta::AeroNamespaceUri()
            : CanonicalXamlNamespace(xamlNamespace),
        CanonicalXamlTypeName(localName));
    if (type == nullptr) return TypeNotFound();
    return SchemaTypeInfo{type->id, type->kind, type->flags};
}

Base::Result<ResolvedMember> SchemaManifest::ResolveMember(
    Meta::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* target = state_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) return MemberNotFound();

    Base::Result<ParsedXamlMemberName> parsed =
        ParseXamlMemberName(name.LocalName(), name.NamespaceUri(), target->xamlNamespace.View());
    if (!parsed) return parsed.GetStatus();

    const ParsedXamlMemberName& member = parsed.Value();
    if (!member.hasOwner) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->xamlNamespace.View()) {
            return MemberNotFound();
        }
        Base::Result<ResolvedMember> resolved = state_->ResolvePropertyOrEvent(
            targetType, targetType, member.memberName, syntax, false);
        if (resolved || member.memberName != Base::StringView("ToolTip")) {
            return resolved;
        }

        // WPF exposes ToolTip as a FrameworkElement property even though the
        // storage and display policy live in ToolTipService.
        const SchemaManifest::State::TypeRecord* service =
            state_->FindType(Meta::AeroNamespaceUri(), "ToolTipService");
        if (service == nullptr) return resolved.GetStatus();
        return state_->ResolvePropertyOrEvent(
            targetType, service->id, member.memberName, syntax, false);
    }

    if (IsAeroExtensionsFacade(
            member.ownerNamespace, member.ownerName)) {
        const SchemaManifest::State::TypeRecord* aeroOwner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
        if (aeroOwner != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, member.memberName, syntax, true);
        }
        return state_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            member.memberName,
            syntax,
            false);
    }
    const SchemaManifest::State::TypeRecord* owner = state_->FindType(
        CanonicalXamlNamespace(member.ownerNamespace),
        CanonicalXamlTypeName(member.ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(member.ownerName));
    }
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (member.memberName == Base::StringView("ContextMenu")) {
        const SchemaManifest::State::TypeRecord* service = state_->FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, service->id, member.memberName, syntax, true);
        }
    }
    return state_->ResolvePropertyOrEvent(
        targetType, owner->id, member.memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifest::State::TypeRecord* type = state_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const SchemaManifest::State::MemberRecord* member = state_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    return MakeResolvedPropertyMember(
        member->id,
        member->ownerType,
        member->valueType,
        static_cast<Meta::PropertyFlags>(member->flags));
}

} // namespace Aero::Markup



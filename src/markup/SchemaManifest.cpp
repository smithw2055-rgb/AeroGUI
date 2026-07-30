#include <Aero/Markup/Schema.hpp>

// Immutable compiled-schema manifest implementation.

#include <Aero/Base/Assert.hpp>
#include <Aero/Markup/Resources.hpp>
#include <Aero/Markup/Schema.hpp>

#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

constexpr std::uint32_t ManifestMagic = UINT32_C(0x48435341); // ASCH
constexpr std::uint32_t ManifestEncodingVersion = 1U;

enum class ManifestMemberKind : std::uint8_t {
    Property = 0U,
    Event
};

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

bool HasPropertyFlag(
    Core::PropertyFlags value,
    Core::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(
    Core::EventFlags value,
    Core::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    constexpr Base::StringView AeroExtensionsPrefix(
        "clr-namespace:AeroGUIExtensions");
    const bool aeroExtensions = value.SizeBytes() >=
            AeroExtensionsPrefix.SizeBytes() &&
        value.Substr(0U, AeroExtensionsPrefix.SizeBytes()) ==
            AeroExtensionsPrefix;
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace || aeroExtensions
        ? Core::AeroNamespaceUri()
        : value;
}

bool IsSystemNamespace(
    Base::StringView value) noexcept {
    return value.SizeBytes() >=
            SystemNamespacePrefix.SizeBytes() &&
        value.Substr(0U, SystemNamespacePrefix.SizeBytes()) ==
            SystemNamespacePrefix;
}

Base::StringView CanonicalXamlTypeName(
    Base::StringView value) noexcept {
    return value == Base::StringView("HierarchicalDataTemplate")
        ? Base::StringView("DataTemplate")
        : value;
}

bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    constexpr Base::StringView Prefix(
        "clr-namespace:AeroGUIExtensions");
    const bool namespaceMatches =
        xamlNamespace.SizeBytes() >=
            Prefix.SizeBytes() &&
        xamlNamespace.Substr(
            0U, Prefix.SizeBytes()) == Prefix;
    return namespaceMatches &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
          ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element"));
}

Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    return output.TryPushBack(value);
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        Base::Result<void> appended = output.TryPushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        Base::Result<void> appended = output.TryPushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> result = AppendU32(output, value.SizeBytes());
    if (!result) return result.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        result = AppendU8(
            output,
            static_cast<std::uint8_t>(value[index]));
        if (!result) return result.GetStatus();
    }
    return {};
}

class Decoder final {
public:
    explicit Decoder(Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes > maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML schema manifest string bounds are invalid");
        }
        Base::String value(&allocator);
        Base::Result<void> assigned = value.TryAssign(
            Base::StringView(
                reinterpret_cast<const char*>(bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }

private:
    static Base::Status Truncated() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest payload is truncated");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
};

} // namespace

struct SchemaManifest::Impl final {
    struct TypeRecord final {
        explicit TypeRecord(Base::IAllocator& allocator) noexcept
            : xamlNamespace(&allocator), name(&allocator) {}

        Core::TypeId id = Core::InvalidTypeId;
        Core::TypeId baseType = Core::InvalidTypeId;
        Core::MetadataTypeKind kind = Core::MetadataTypeKind::Object;
        Core::TypeFlags flags = Core::TypeFlags::None;
        Core::MemberId contentMember = Core::InvalidMemberId;
        Base::String xamlNamespace;
        Base::String name;
    };

    struct MemberRecord final {
        explicit MemberRecord(Base::IAllocator& allocator) noexcept
            : name(&allocator) {}

        Core::MemberId id = Core::InvalidMemberId;
        ManifestMemberKind kind = ManifestMemberKind::Property;
        Core::TypeId ownerType = Core::InvalidTypeId;
        Core::TypeId valueType = Core::InvalidTypeId;
        std::uint32_t flags = 0U;
        Base::String name;
    };

    explicit Impl(Base::IAllocator& allocator) noexcept
        : types(&allocator),
          members(&allocator),
          typeIndex(&allocator),
          memberIndex(&allocator) {}

    CompiledCacheIdentity identity;
    Base::Vector<TypeRecord> types;
    Base::Vector<MemberRecord> members;
    Base::HashMap<Core::TypeId, std::uint32_t> typeIndex;
    Base::HashMap<Core::MemberId, std::uint32_t> memberIndex;
    bool valid = false;

    Base::Result<void> RebuildIndexes() noexcept {
        typeIndex.Clear();
        memberIndex.Clear();
        for (std::uint32_t index = 0U; index < types.Size(); ++index) {
            Base::Result<typename Base::HashMap<Core::TypeId, std::uint32_t>::InsertResult>
                inserted = typeIndex.TryInsert(types[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            Base::Result<typename Base::HashMap<Core::MemberId, std::uint32_t>::InsertResult>
                inserted = memberIndex.TryInsert(members[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate MemberId values");
            }
        }
        return {};
    }

    const TypeRecord* FindType(Core::TypeId id) const noexcept {
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

    const MemberRecord* FindMember(Core::MemberId id) const noexcept {
        const std::uint32_t* index = memberIndex.Find(id);
        return index != nullptr && *index < members.Size()
            ? &members[*index] : nullptr;
    }

    const MemberRecord* FindMember(
        Core::TypeId ownerType,
        Base::StringView name,
        ManifestMemberKind kind,
        bool includeBaseTypes) const noexcept {
        Core::TypeId current = ownerType;
        for (std::uint32_t depth = 0U;
             current != Core::InvalidTypeId && depth <= types.Size();
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
        Core::TypeId type,
        Core::TypeId expectedBase) const noexcept {
        if (type == Core::InvalidTypeId ||
            expectedBase == Core::InvalidTypeId) {
            return false;
        }
        Core::TypeId current = type;
        for (std::uint32_t depth = 0U;
             current != Core::InvalidTypeId && depth <= types.Size();
             ++depth) {
            if (current == expectedBase) return true;
            const TypeRecord* descriptor = FindType(current);
            if (descriptor == nullptr) return false;
            current = descriptor->baseType;
        }
        return false;
    }

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
        Core::TypeId targetType,
        Core::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept {
        const MemberRecord* property = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Property,
            true);
        if (property != nullptr) {
            const Core::PropertyFlags flags =
                static_cast<Core::PropertyFlags>(property->flags);
            const bool attached = HasPropertyFlag(
                flags, Core::PropertyFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached) {
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
            ResolvedMember resolved;
            resolved.id = property->id;
            resolved.kind = Core::MemberKind::Property;
            resolved.ownerType = property->ownerType;
            resolved.valueType = property->valueType;
            resolved.propertyFlags = flags;
            resolved.attached = attached;
            return resolved;
        }

        const MemberRecord* event = FindMember(
            ownerType,
            memberName,
            ManifestMemberKind::Event,
            true);
        if (event != nullptr) {
            const Core::EventFlags flags =
                static_cast<Core::EventFlags>(event->flags);
            const bool attached = HasEventFlag(
                flags, Core::EventFlags::Attached);
            if (ownerWasExplicit &&
                syntax == MemberSyntax::Attribute && !attached) {
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
            ResolvedMember resolved;
            resolved.id = event->id;
            resolved.kind = Core::MemberKind::Event;
            resolved.ownerType = event->ownerType;
            resolved.valueType = event->valueType;
            resolved.eventFlags = flags;
            resolved.attached = attached;
            return resolved;
        }
        return MemberNotFound();
    }
};

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

void DestroyImpl(
    Base::IAllocator& allocator,
    SchemaManifest::Impl*& impl) noexcept {
    if (impl == nullptr) return;
    impl->~Impl();
    allocator.Deallocate(
        impl,
        sizeof(SchemaManifest::Impl),
        alignof(SchemaManifest::Impl),
        Base::MemoryTag::Markup);
    impl = nullptr;
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
    result = AppendU32(output, identity.metadataRuntimeFormatVersion);
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
    identity.metadataRuntimeFormatVersion = value.Value();
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
    Impl* impl) noexcept
    : allocator_(&allocator), impl_(impl) {}

SchemaManifest::~SchemaManifest() noexcept {
    if (allocator_ != nullptr) DestroyImpl(*allocator_, impl_);
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = &Base::GetDefaultAllocator();
    other.impl_ = nullptr;
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (allocator_ != nullptr) DestroyImpl(*allocator_, impl_);
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = &Base::GetDefaultAllocator();
    other.impl_ = nullptr;
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
    Base::Result<Impl*> created = AllocateObject<Impl>(selected);
    if (!created) return created.GetStatus();
    Impl* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyImpl(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Core::TypeRegistry& descriptors = schema.Types();
    Base::Result<void> reserved = impl->types.TryReserve(descriptors.TypeCount());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }
    reserved = impl->members.TryReserve(
        descriptors.PropertyCount() + descriptors.EventCount());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }

    for (const Core::TypeInfo& type : descriptors.Types()) {
        Impl::TypeRecord record(selected);
        record.id = type.Id();
        record.baseType = type.BaseType();
        record.kind = type.Kind();
        record.flags = type.Flags();
        Base::Result<void> assigned = record.xamlNamespace.TryAssign(
            type.XamlNamespace());
        if (assigned) assigned = record.name.TryAssign(type.Name());
        if (!assigned) {
            DestroyImpl(selected, impl);
            return assigned.GetStatus();
        }
        Base::Result<ResolvedMember> content =
            schema.ResolveContentMember(type.Id());
        if (content) {
            record.contentMember = content.Value().id;
        } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
            DestroyImpl(selected, impl);
            return content.GetStatus();
        }
        Base::Result<void> appended = impl->types.TryPushBack(
            std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }

        for (const Core::PropertyInfo& property : type.Properties()) {
            Impl::MemberRecord member(selected);
            member.id = property.Id();
            member.kind = ManifestMemberKind::Property;
            member.ownerType = property.OwnerType();
            member.valueType = property.ValueType();
            member.flags = static_cast<std::uint32_t>(property.Flags());
            assigned = member.name.TryAssign(property.Name());
            if (!assigned) {
                DestroyImpl(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.TryPushBack(std::move(member));
            if (!appended) {
                DestroyImpl(selected, impl);
                return appended.GetStatus();
            }
        }

        for (const Core::EventInfo& event : type.Events()) {
            Impl::MemberRecord member(selected);
            member.id = event.Id();
            member.kind = ManifestMemberKind::Event;
            member.ownerType = event.OwnerType();
            member.valueType = event.EventArgsType();
            member.flags = static_cast<std::uint32_t>(event.Flags());
            assigned = member.name.TryAssign(event.Name());
            if (!assigned) {
                DestroyImpl(selected, impl);
                return assigned.GetStatus();
            }
            appended = impl->members.TryPushBack(std::move(member));
            if (!appended) {
                DestroyImpl(selected, impl);
                return appended.GetStatus();
            }
        }
    }

    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyImpl(selected, impl);
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
    Decoder decoder(bytes);
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

    Base::Result<Impl*> created = AllocateObject<Impl>(selected);
    if (!created) return created.GetStatus();
    Impl* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity = ReadIdentity(decoder);
    if (!identity) {
        DestroyImpl(selected, impl);
        return identity.GetStatus();
    }
    CompiledCacheIdentity current;
    current.metadataSchemaHash = identity.Value().metadataSchemaHash;
    if (CompareCompiledCacheIdentity(identity.Value(), current) !=
        CompiledCacheCompatibility::Compatible) {
        DestroyImpl(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML schema manifest ABI is incompatible with this tool");
    }
    impl->identity = identity.Value();

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) {
        DestroyImpl(selected, impl);
        return typeCount.GetStatus();
    }
    Base::Result<std::uint32_t> memberCount = decoder.ReadU32();
    if (!memberCount) {
        DestroyImpl(selected, impl);
        return memberCount.GetStatus();
    }
    if (typeCount.Value() > limits.maxTypes ||
        memberCount.Value() > limits.maxMembers) {
        DestroyImpl(selected, impl);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML schema manifest descriptor count exceeds limits");
    }
    Base::Result<void> reserved = impl->types.TryReserve(typeCount.Value());
    if (reserved) reserved = impl->members.TryReserve(memberCount.Value());
    if (!reserved) {
        DestroyImpl(selected, impl);
        return reserved.GetStatus();
    }

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        Impl::TypeRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyImpl(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint64_t> baseType = decoder.ReadU64();
        if (!baseType) {
            DestroyImpl(selected, impl);
            return baseType.GetStatus();
        }
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) {
            DestroyImpl(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyImpl(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<std::uint64_t> content = decoder.ReadU64();
        if (!content) {
            DestroyImpl(selected, impl);
            return content.GetStatus();
        }
        Base::Result<Base::String> xamlNamespace = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!xamlNamespace) {
            DestroyImpl(selected, impl);
            return xamlNamespace.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyImpl(selected, impl);
            return name.GetStatus();
        }
        if (id.Value() == Core::InvalidTypeId || name.Value().Empty() ||
            Core::MakeTypeId(
                xamlNamespace.Value().View(),
                name.Value().View()) != id.Value() ||
            kind.Value() > static_cast<std::uint32_t>(Core::MetadataTypeKind::Primitive)) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest type descriptor is invalid");
        }
        record.id = id.Value();
        record.baseType = baseType.Value();
        record.kind = static_cast<Core::MetadataTypeKind>(kind.Value());
        record.flags = static_cast<Core::TypeFlags>(flags.Value());
        record.contentMember = content.Value();
        record.xamlNamespace = std::move(xamlNamespace).Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->types.TryPushBack(std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        Impl::MemberRecord record(selected);
        Base::Result<std::uint64_t> id = decoder.ReadU64();
        if (!id) {
            DestroyImpl(selected, impl);
            return id.GetStatus();
        }
        Base::Result<std::uint8_t> kind = decoder.ReadU8();
        if (!kind) {
            DestroyImpl(selected, impl);
            return kind.GetStatus();
        }
        Base::Result<std::uint32_t> reservedField = decoder.ReadU32();
        if (!reservedField) {
            DestroyImpl(selected, impl);
            return reservedField.GetStatus();
        }
        Base::Result<std::uint64_t> owner = decoder.ReadU64();
        if (!owner) {
            DestroyImpl(selected, impl);
            return owner.GetStatus();
        }
        Base::Result<std::uint64_t> valueType = decoder.ReadU64();
        if (!valueType) {
            DestroyImpl(selected, impl);
            return valueType.GetStatus();
        }
        Base::Result<std::uint32_t> flags = decoder.ReadU32();
        if (!flags) {
            DestroyImpl(selected, impl);
            return flags.GetStatus();
        }
        Base::Result<Base::String> name = decoder.ReadString(
            selected, totalStringBytes, limits.maxStringBytes);
        if (!name) {
            DestroyImpl(selected, impl);
            return name.GetStatus();
        }
        const bool validKind =
            kind.Value() <= static_cast<std::uint8_t>(ManifestMemberKind::Event);
        const Core::MemberKind metadataKind =
            kind.Value() == static_cast<std::uint8_t>(ManifestMemberKind::Event)
            ? Core::MemberKind::Event
            : Core::MemberKind::Property;
        if (id.Value() == Core::InvalidMemberId ||
            owner.Value() == Core::InvalidTypeId ||
            valueType.Value() == Core::InvalidTypeId ||
            name.Value().Empty() ||
            reservedField.Value() != 0U ||
            !validKind ||
            Core::MakeMemberId(
                owner.Value(), metadataKind, name.Value().View()) != id.Value()) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest member descriptor is invalid");
        }
        record.id = id.Value();
        record.kind = static_cast<ManifestMemberKind>(kind.Value());
        record.ownerType = owner.Value();
        record.valueType = valueType.Value();
        record.flags = flags.Value();
        record.name = std::move(name).Value();
        Base::Result<void> appended = impl->members.TryPushBack(std::move(record));
        if (!appended) {
            DestroyImpl(selected, impl);
            return appended.GetStatus();
        }
    }

    if (!decoder.AtEnd()) {
        DestroyImpl(selected, impl);
        return InvalidManifest("XAML schema manifest has trailing bytes");
    }
    Base::Result<void> indexed = impl->RebuildIndexes();
    if (!indexed) {
        DestroyImpl(selected, impl);
        return indexed.GetStatus();
    }
    for (const Impl::TypeRecord& type : impl->types) {
        if (type.baseType != Core::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Core::TypeId current = type.id;
        std::uint32_t depth = 0U;
        while (current != Core::InvalidTypeId && depth <= impl->types.Size()) {
            const Impl::TypeRecord* currentType = impl->FindType(current);
            if (currentType == nullptr) break;
            current = currentType->baseType;
            ++depth;
        }
        if (current != Core::InvalidTypeId) {
            DestroyImpl(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Core::InvalidMemberId) {
            const Impl::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyImpl(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const Impl::MemberRecord& member : impl->members) {
        if (impl->FindType(member.ownerType) == nullptr ||
            impl->FindType(member.valueType) == nullptr) {
            DestroyImpl(selected, impl);
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
    if (result) result = AppendIdentity(output, impl_->identity);
    if (result) result = AppendU32(output, impl_->types.Size());
    if (result) result = AppendU32(output, impl_->members.Size());
    if (!result) return result.GetStatus();

    for (const Impl::TypeRecord& type : impl_->types) {
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
    for (const Impl::MemberRecord& member : impl_->members) {
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
    return impl_ != nullptr && impl_->valid;
}

std::uint32_t SchemaManifest::TypeCount() const noexcept {
    return IsValid() ? impl_->types.Size() : 0U;
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    return IsValid() ? impl_->members.Size() : 0U;
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return impl_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const Impl::TypeRecord* type = impl_->FindType(
        IsSystemNamespace(xamlNamespace) &&
            (localName == Base::StringView("String") ||
             localName == Base::StringView("Double"))
            ? Core::AeroNamespaceUri()
            : CanonicalXamlNamespace(xamlNamespace),
        CanonicalXamlTypeName(localName));
    if (type == nullptr) return TypeNotFound();
    return SchemaTypeInfo{type->id, type->kind, type->flags};
}

Base::Result<ResolvedMember> SchemaManifest::ResolveMember(
    Core::TypeId targetType,
    const QualifiedName& name,
    MemberSyntax syntax) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const Impl::TypeRecord* target = impl_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) return MemberNotFound();

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML schema manifest member contains multiple owner separators");
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            CanonicalXamlNamespace(name.NamespaceUri()) !=
                target->xamlNamespace.View()) {
            return MemberNotFound();
        }
        return impl_->ResolvePropertyOrEvent(
            targetType, targetType, localName, syntax, false);
    }
    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML schema manifest member owner syntax is invalid");
    }
    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U, localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->xamlNamespace.View() : name.NamespaceUri();
    if (IsAeroExtensionsFacade(
            ownerNamespace, ownerName)) {
        // The reference Gallery uses the legacy Element.BlendingMode
        // extension name. Element is also a real Aero extension owner
        // (PPAAOut), so normalize this one compatibility alias before
        // looking up the owner rather than letting that type shadow the
        // inherited UIElement BlendMode property.
        if (ownerName == Base::StringView("Element") &&
            memberName == Base::StringView("BlendingMode")) {
            return impl_->ResolvePropertyOrEvent(
                targetType,
                targetType,
                Base::StringView("BlendMode"),
                syntax,
                false);
        }
        // The legacy AeroGUIExtensions facade predates real attached
        // properties. Prefer a registered Aero owner (for example
        // aero:Path.TrimEnd) and retain the facade only for extension-only
        // members such as aero:Text.*.
        const Impl::TypeRecord* aeroOwner = impl_->FindType(
            Core::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return impl_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, memberName, syntax, true);
        }
        return impl_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const Impl::TypeRecord* owner = impl_->FindType(
        CanonicalXamlNamespace(ownerNamespace),
        CanonicalXamlTypeName(ownerName));
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (memberName == Base::StringView("ContextMenu")) {
        const Impl::TypeRecord* service = impl_->FindType(
            Core::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return impl_->ResolvePropertyOrEvent(
                targetType, service->id, memberName, syntax, true);
        }
    }
    return impl_->ResolvePropertyOrEvent(
        targetType, owner->id, memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Core::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const Impl::TypeRecord* type = impl_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const Impl::MemberRecord* member = impl_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    const Core::PropertyFlags flags =
        static_cast<Core::PropertyFlags>(member->flags);
    ResolvedMember resolved;
    resolved.id = member->id;
    resolved.kind = Core::MemberKind::Property;
    resolved.ownerType = member->ownerType;
    resolved.valueType = member->valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Core::PropertyFlags::Attached);
    return resolved;
}

} // namespace Aero::Markup

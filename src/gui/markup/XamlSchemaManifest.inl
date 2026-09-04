// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.

#include <Aero/Base/Assert.hpp>

#include <new>

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
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(
    Meta::EventFlags value,
    Meta::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

constexpr Base::StringView WpfPresentationNamespace(
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation");
constexpr Base::StringView BehaviorsNamespace(
    "http://schemas.microsoft.com/xaml/behaviors");
constexpr Base::StringView BlendInteractivityNamespace(
    "http://schemas.microsoft.com/expression/2010/interactivity");
constexpr Base::StringView SystemNamespacePrefix(
    "clr-namespace:System");

bool MatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

bool IsExtensionsClrNamespace(Base::StringView value) noexcept {
    return MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:AeroGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Aero.GUI.Extensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:NoesisGUIExtensions")) ||
        MatchesClrNamespacePrefix(
            value, Base::StringView("clr-namespace:Noesis.GUI.Extensions"));
}

Base::StringView CanonicalXamlNamespace(
    Base::StringView value) noexcept {
    return value == WpfPresentationNamespace ||
            value == BehaviorsNamespace ||
            value == BlendInteractivityNamespace ||
            IsExtensionsClrNamespace(value)
        ? Meta::AeroNamespaceUri()
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
    if (value == Base::StringView("Geometry")) {
        return Base::StringView("StreamGeometry");
    }
    if (value == Base::StringView("VisualStateTransition")) {
        return Base::StringView("VisualTransition");
    }
    if (value == Base::StringView("MonochromeBrush")) {
        return Base::StringView("MonochromeShader");
    }
    if (value == Base::StringView("ConicGradientBrush")) {
        return Base::StringView("ConicGradientShader");
    }
    if (value == Base::StringView("WavesBrush")) {
        return Base::StringView("WavesShader");
    }
    return value;
}

bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    return IsExtensionsClrNamespace(xamlNamespace) &&
        (ownerName == Base::StringView("Text") ||
         ownerName == Base::StringView("Path") ||
         ownerName == Base::StringView("Brush") ||
          ownerName == Base::StringView("Element") ||
          ownerName == Base::StringView("RichText"));
}

Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    return output.PushBack(value);
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
            static_cast<std::uint8_t>(value >> shift));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        Base::Result<void> appended = output.PushBack(
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

class Decoder {
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
        Base::Result<void> assigned = value.Assign(
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

struct SchemaManifestState {
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

    explicit SchemaManifestState(Base::IAllocator& allocator) noexcept
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
            Base::Result<typename Base::HashMap<Meta::TypeId, std::uint32_t>::InsertResult>
                inserted = typeIndex.Insert(types[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
                return InvalidManifest("XAML schema manifest contains duplicate TypeId values");
            }
        }
        for (std::uint32_t index = 0U; index < members.Size(); ++index) {
            Base::Result<typename Base::HashMap<Meta::MemberId, std::uint32_t>::InsertResult>
                inserted = memberIndex.Insert(members[index].id, index);
            if (!inserted) return inserted.GetStatus();
            if (!inserted.Value().inserted) {
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
            ResolvedMember resolved;
            resolved.id = property->id;
            resolved.kind = Meta::MemberKind::Property;
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
            ResolvedMember resolved;
            resolved.id = event->id;
            resolved.kind = Meta::MemberKind::Event;
            resolved.ownerType = event->ownerType;
            resolved.valueType = event->valueType;
            resolved.eventFlags = flags;
            resolved.attached = attached ||
                (ownerWasExplicit &&
                 syntax == MemberSyntax::Attribute && routed);
            return resolved;
        }
        return MemberNotFound();
    }
};

static_assert(
    sizeof(SchemaManifestState) <= 2048,
    "SchemaManifest inline state storage is too small");
static_assert(
    alignof(SchemaManifestState) <= alignof(std::max_align_t),
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
    SchemaManifestState*& state) noexcept {
    if (state == nullptr) return;
    state->~SchemaManifestState();
    allocator.Deallocate(
        state,
        sizeof(SchemaManifestState),
        alignof(SchemaManifestState),
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
    SchemaManifestState* state) noexcept
    : allocator_(&allocator) {
    if (state == nullptr) return;
    state_ = new (stateStorage_) SchemaManifestState(std::move(*state));
    DestroyManifestState(allocator, state);
}

SchemaManifest::~SchemaManifest() noexcept {
    if (state_ == nullptr) return;
    state_->~SchemaManifestState();
    state_ = nullptr;
}

SchemaManifest::SchemaManifest(
    SchemaManifest&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            SchemaManifestState(std::move(*other.state_));
        other.state_->~SchemaManifestState();
        other.state_ = nullptr;
    }
    other.allocator_ = &Base::GetDefaultAllocator();
}

SchemaManifest& SchemaManifest::operator=(
    SchemaManifest&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~SchemaManifestState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            SchemaManifestState(std::move(*other.state_));
        other.state_->~SchemaManifestState();
        other.state_ = nullptr;
    }
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
    Base::Result<SchemaManifestState*> created = AllocateObject<SchemaManifestState>(selected);
    if (!created) return created.GetStatus();
    SchemaManifestState* impl = created.Value();

    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) {
        DestroyManifestState(selected, impl);
        return identity.GetStatus();
    }
    impl->identity = identity.Value();

    const Meta::TypeRegistry& descriptors = schema.Types();
    Base::Result<void> reserved = impl->types.Reserve(descriptors.TypeCount());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }
    reserved = impl->members.Reserve(
        descriptors.PropertyCount() + descriptors.EventCount());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }

    for (const Meta::TypeInfo& type : descriptors.Types()) {
        SchemaManifestState::TypeRecord record(selected);
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
        Base::Result<void> appended = impl->types.PushBack(
            std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }

        for (const Meta::PropertyInfo& property : type.Properties()) {
            SchemaManifestState::MemberRecord member(selected);
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
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyManifestState(selected, impl);
                return appended.GetStatus();
            }
        }

        for (const Meta::EventInfo& event : type.Events()) {
            SchemaManifestState::MemberRecord member(selected);
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
            appended = impl->members.PushBack(std::move(member));
            if (!appended) {
                DestroyManifestState(selected, impl);
                return appended.GetStatus();
            }
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

    Base::Result<SchemaManifestState*> created = AllocateObject<SchemaManifestState>(selected);
    if (!created) return created.GetStatus();
    SchemaManifestState* impl = created.Value();

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
    Base::Result<void> reserved = impl->types.Reserve(typeCount.Value());
    if (reserved) reserved = impl->members.Reserve(memberCount.Value());
    if (!reserved) {
        DestroyManifestState(selected, impl);
        return reserved.GetStatus();
    }

    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        SchemaManifestState::TypeRecord record(selected);
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
        Base::Result<void> appended = impl->types.PushBack(std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }
    }

    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        SchemaManifestState::MemberRecord record(selected);
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
        Base::Result<void> appended = impl->members.PushBack(std::move(record));
        if (!appended) {
            DestroyManifestState(selected, impl);
            return appended.GetStatus();
        }
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
    for (const SchemaManifestState::TypeRecord& type : impl->types) {
        if (type.baseType != Meta::InvalidTypeId &&
            impl->FindType(type.baseType) == nullptr) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest base type is missing");
        }
        Meta::TypeId current = type.id;
        std::uint32_t depth = 0U;
        while (current != Meta::InvalidTypeId && depth <= impl->types.Size()) {
            const SchemaManifestState::TypeRecord* currentType = impl->FindType(current);
            if (currentType == nullptr) break;
            current = currentType->baseType;
            ++depth;
        }
        if (current != Meta::InvalidTypeId) {
            DestroyManifestState(selected, impl);
            return InvalidManifest("XAML schema manifest type hierarchy contains a cycle");
        }
        if (type.contentMember != Meta::InvalidMemberId) {
            const SchemaManifestState::MemberRecord* content = impl->FindMember(type.contentMember);
            if (content == nullptr ||
                content->kind != ManifestMemberKind::Property ||
                !impl->IsDerivedFrom(type.id, content->ownerType)) {
                DestroyManifestState(selected, impl);
                return InvalidManifest("XAML schema manifest content member is missing or incompatible");
            }
        }
    }
    for (const SchemaManifestState::MemberRecord& member : impl->members) {
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

    for (const SchemaManifestState::TypeRecord& type : state_->types) {
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
    for (const SchemaManifestState::MemberRecord& member : state_->members) {
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
    const SchemaManifestState::TypeRecord* type = state_->FindType(
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
    const SchemaManifestState::TypeRecord* target = state_->FindType(targetType);
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
        return state_->ResolvePropertyOrEvent(
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
            return state_->ResolvePropertyOrEvent(
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
        const SchemaManifestState::TypeRecord* aeroOwner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
        if (aeroOwner != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, aeroOwner->id, memberName, syntax, true);
        }
        return state_->ResolvePropertyOrEvent(
            targetType,
            targetType,
            memberName,
            syntax,
            false);
    }
    const SchemaManifestState::TypeRecord* owner = state_->FindType(
        CanonicalXamlNamespace(ownerNamespace),
        CanonicalXamlTypeName(ownerName));
    if (owner == nullptr && name.NamespaceUri().Empty()) {
        owner = state_->FindType(
            Meta::AeroNamespaceUri(),
            CanonicalXamlTypeName(ownerName));
    }
    if (owner == nullptr) return MemberNotFound();
    // WPF exposes ContextMenu through FrameworkElement property-element
    // syntax (for example Border.ContextMenu) while storage is supplied by
    // the attached ContextMenuService property.
    if (memberName == Base::StringView("ContextMenu")) {
        const SchemaManifestState::TypeRecord* service = state_->FindType(
            Meta::AeroNamespaceUri(), "ContextMenuService");
        if (service != nullptr) {
            return state_->ResolvePropertyOrEvent(
                targetType, service->id, memberName, syntax, true);
        }
    }
    return state_->ResolvePropertyOrEvent(
        targetType, owner->id, memberName, syntax, true);
}

Base::Result<ResolvedMember> SchemaManifest::ResolveContentMember(
    Meta::TypeId targetType) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    const SchemaManifestState::TypeRecord* type = state_->FindType(targetType);
    if (type == nullptr) return TypeNotFound();
    if (type->contentMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML schema manifest type has no content member");
    }
    const SchemaManifestState::MemberRecord* member = state_->FindMember(type->contentMember);
    if (member == nullptr || member->kind != ManifestMemberKind::Property) {
        return InvalidManifest("XAML schema manifest content member is invalid");
    }
    const Meta::PropertyFlags flags =
        static_cast<Meta::PropertyFlags>(member->flags);
    ResolvedMember resolved;
    resolved.id = member->id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = member->ownerType;
    resolved.valueType = member->valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

} // namespace Aero::Markup



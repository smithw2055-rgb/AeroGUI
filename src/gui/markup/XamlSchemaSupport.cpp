#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EnumRegistration.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/templates/TemplateInstance.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/VisualStateManager.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

// ============================================================================
// SECTION: XamlSchemaManifest
// Source: src/gui/markup/XamlSchemaManifest.cpp
// ============================================================================

// ===== SchemaManifest =====


// Immutable compiled-schema manifest implementation.



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
    const Schema* liveSchema = nullptr;
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
        if (const TypeRecord* byId = FindType(Meta::MakeTypeId(xamlNamespace, name))) {
            if (byId->xamlNamespace.View() == xamlNamespace &&
                byId->name.View() == name) {
                return byId;
            }
        }
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
        const Meta::MemberKind metadataKind =
            kind == ManifestMemberKind::Event
                ? Meta::MemberKind::Event
                : Meta::MemberKind::Property;
        Meta::TypeId current = ownerType;
        for (std::uint32_t depth = 0U;
             current != Meta::InvalidTypeId && depth <= types.Size();
             ++depth) {
            if (const MemberRecord* byId = FindMember(
                    Meta::MakeMemberId(current, metadataKind, name))) {
                if (byId->ownerType == current &&
                    byId->kind == kind &&
                    byId->name.View() == name) {
                    return byId;
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
    return AppendCompiledCacheIdentity(output, identity);
}

Base::Result<CompiledCacheIdentity> ReadIdentity(
    Decoder& decoder) noexcept {
    return ReadCompiledCacheIdentity(decoder);
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
    impl->liveSchema = &schema;
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
    if (state_->liveSchema != nullptr) {
        const Meta::TypeRegistry& descriptors = state_->liveSchema->Types();
        if (result) result = AppendU32(output, descriptors.TypeCount());
        if (result) {
            result = AppendU32(
                output,
                descriptors.PropertyCount() + descriptors.EventCount());
        }
        if (!result) return result.GetStatus();
        for (const Meta::TypeInfo& type : descriptors.Types()) {
            Meta::MemberId contentMember = Meta::InvalidMemberId;
            Base::Result<ResolvedMember> content =
                state_->liveSchema->ResolveContentMember(type.Id());
            if (content) {
                contentMember = content.Value().id;
            } else if (content.GetStatus().code != Base::ErrorCode::NotFound) {
                return content.GetStatus();
            }
            result = AppendU64(output, type.Id());
            if (result) result = AppendU64(output, type.BaseType());
            if (result) result = AppendU32(output, static_cast<std::uint32_t>(type.Kind()));
            if (result) result = AppendU32(output, static_cast<std::uint32_t>(type.Flags()));
            if (result) result = AppendU64(output, contentMember);
            if (result) result = AppendString(output, type.XamlNamespace());
            if (result) result = AppendString(output, type.Name());
            if (!result) return result.GetStatus();
        }
        for (const Meta::TypeInfo& type : descriptors.Types()) {
            for (const Meta::PropertyInfo& property : type.Properties()) {
                result = AppendU64(output, property.Id());
                if (result) result = AppendU8(output, static_cast<std::uint8_t>(ManifestMemberKind::Property));
                if (result) result = AppendU32(output, 0U);
                if (result) result = AppendU64(output, property.OwnerType());
                if (result) result = AppendU64(output, property.ValueType());
                if (result) result = AppendU32(output, static_cast<std::uint32_t>(property.Flags()));
                if (result) result = AppendString(output, property.Name());
                if (!result) return result.GetStatus();
            }
            for (const Meta::EventInfo& event : type.Events()) {
                result = AppendU64(output, event.Id());
                if (result) result = AppendU8(output, static_cast<std::uint8_t>(ManifestMemberKind::Event));
                if (result) result = AppendU32(output, 0U);
                if (result) result = AppendU64(output, event.OwnerType());
                if (result) result = AppendU64(output, event.EventArgsType());
                if (result) result = AppendU32(output, static_cast<std::uint32_t>(event.Flags()));
                if (result) result = AppendString(output, event.Name());
                if (!result) return result.GetStatus();
            }
        }
        return output;
    }

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
    if (!IsValid()) return 0U;
    if (state_->liveSchema != nullptr) {
        return state_->liveSchema->Types().TypeCount();
    }
    return state_->types.Size();
}

std::uint32_t SchemaManifest::MemberCount() const noexcept {
    if (!IsValid()) return 0U;
    if (state_->liveSchema != nullptr) {
        const Meta::TypeRegistry& descriptors = state_->liveSchema->Types();
        return descriptors.PropertyCount() + descriptors.EventCount();
    }
    return state_->members.Size();
}

const CompiledCacheIdentity& SchemaManifest::Identity() const noexcept {
    AERO_ASSERT(IsValid());
    return state_->identity;
}

Base::Result<SchemaTypeInfo> SchemaManifest::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!IsValid()) return ManifestNotReady();
    if (state_->liveSchema != nullptr) {
        Base::Result<const Meta::TypeInfo*> resolved =
            state_->liveSchema->ResolveType(xamlNamespace, localName);
        if (!resolved) return resolved.GetStatus();
        return SchemaTypeInfo{
            resolved.Value()->Id(),
            resolved.Value()->Kind(),
            resolved.Value()->Flags()};
    }
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
    if (state_->liveSchema != nullptr) {
        return state_->liveSchema->ResolveMember(targetType, name, syntax);
    }
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
    if (state_->liveSchema != nullptr) {
        return state_->liveSchema->ResolveContentMember(targetType);
    }
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



// ============================================================================
// SECTION: StyleSupport
// Source: src/gui/markup/StyleSupport.cpp
// ============================================================================

// ===== StyleSupport =====






namespace Aero::Markup {

namespace {

Base::Status InvalidStyleXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::Status MissingStyleProperty(
    const char* role,
    Base::StringView property,
    Meta::TypeId targetType,
    const Meta::TypeRegistry& types) noexcept {
    return FormatMissingTargetPropertyStatus(
        "Style", role, property, targetType, types, Base::ErrorCode::NotFound);
}

inline const Meta::DependencyProperty* ResolveStyleProperty(
    const Meta::DependencyPropertyRegistry& properties,
    Meta::TypeId targetType,
    Base::StringView name) noexcept {
    return ResolveXamlDependencyProperty(properties, targetType, name);
}

Base::Result<Meta::PropertyValue> ToPropertyValue(
    const Meta::Value& value,
    Meta::TypeId expectedType) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML style value cannot be represented as a dependency-property value");
    }
    if (value.IsNullObject() && value.Type() != expectedType) {
        return Meta::PropertyValue::NullObject(expectedType);
    }
    return value;
}

Aero::ResourceDictionary* ResolveStyleResources(
    Base::Object& object,
    void*) noexcept {
    return object.RuntimeType() ==
            Aero::Style::StaticTypeId()
        ? &static_cast<Aero::Style&>(object).GetResources()
        : nullptr;
}

Base::Result<Aero::ResourceKey> ResolveStyleImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() != Aero::Style::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Implicit Style key requires a Style object");
    }
    const Meta::TypeId target =
        static_cast<const Aero::Style&>(object).GetTargetType();
    if (target == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Implicit Style requires TargetType");
    }
    return Aero::ResourceKey::FromType(target);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

XamlStyleSchemaFacet::XamlStyleSchemaFacet(
    const UiObjectModelOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlStyleSchemaFacet::Register(
    Schema& schema,
    Meta::TypeId styleType,
    Meta::TypeId setterType,
    Meta::DependencyPropertyHandle styleProperty,
    Meta::TypeId triggerType) noexcept {
    if (schema.IsFrozen() ||
        options_.properties == nullptr || !styleProperty.IsValid() ||
        !options_.properties->IsFrozen() || !options_.properties->Types().IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML Style extension registries are not ready");
    }
    if (schema_ != nullptr ||
        styleType == Meta::InvalidTypeId || setterType == Meta::InvalidTypeId) {
        return InvalidStyleXaml("XAML Style extension registration is invalid");
    }

    const Meta::TypeInfo* styleInfo = schema.Types().FindType(styleType);
    const Meta::TypeInfo* setterInfo = schema.Types().FindType(setterType);
    const Meta::TypeInfo* triggerInfo =
        triggerType != Meta::InvalidTypeId ? schema.Types().FindType(triggerType) : nullptr;
    if (styleInfo == nullptr || setterInfo == nullptr ||
        HasTypeFlag(styleInfo->Flags(), Meta::TypeFlags::ValueType) ||
        HasTypeFlag(setterInfo->Flags(), Meta::TypeFlags::ValueType) ||
        (triggerType != Meta::InvalidTypeId &&
         (triggerInfo == nullptr || HasTypeFlag(triggerInfo->Flags(), Meta::TypeFlags::ValueType)))) {
        return InvalidStyleXaml("XAML Style, Setter, and Trigger must be registered object types");
    }


    const Meta::DependencyProperty* styleDependency = options_.properties->Find(styleProperty);
    if (styleDependency == nullptr || styleDependency->ValueType() != styleType) {
        return InvalidStyleXaml("XAML Style property does not accept the registered Style type");
    }

    schema_ = &schema;
    styleType_ = styleType;
    setterType_ = setterType;
    triggerType_ = triggerType;
    Base::Result<void> styleAdapter =
        schema.AddType({
        styleType_,
        nullptr,
        &EndStyleInit,
        nullptr,
        this,
        false,
        true,
        nullptr,
        nullptr,
        &ResolveStyleResources,
        nullptr,
        false,
        &ResolveStyleImplicitKey});
    if (!styleAdapter) return styleAdapter.GetStatus();
    return {};
}

Base::Result<Meta::PropertyValue> XamlStyleSchemaFacet::ConvertValueForProperty(
    const Meta::Value& value,
    Meta::TypeId targetType,
    Base::StringView propertyName) const noexcept {
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style conversion requires an initialized extension");
    }
    // Setter.Value="{Binding ...}" is a declaration. Converting it to the
    // target DP type (Canvas.Left is double) would drop the Binding and
    // fail style sealing, so ListBoxItem orbit placement never applied.
    if (IsPassthroughMarkupPropertyValue(value)) {
        return value;
    }
    const Meta::DependencyProperty* property =
        ResolveStyleProperty(
            *options_.properties,
            targetType,
            propertyName);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Style property was not found on TargetType");
    }
    const Meta::Value* candidate = &value;
    Base::Result<Meta::Value> converted = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Style conversion was not attempted");
    if (candidate->Kind() == Meta::ValueKind::String) {
        converted = schema_->ConvertText(property->ValueType(), candidate->AsString());
        if (!converted) return converted.GetStatus();
        candidate = &converted.Value();
    }
    Base::Result<Meta::Value> coerced = CoerceXamlValueForProperty(
        *property, *candidate, options_.properties->Types());
    if (!coerced) return coerced.GetStatus();
    return ToPropertyValue(coerced.Value(), property->ValueType());
}

bool SetterValueIsPending(const Aero::Setter* setter) noexcept {
    return setter != nullptr &&
        !setter->GetPropertyName().Empty() &&
        setter->GetAuthoredValue().IsUnset();
}

bool StyleHasPendingResourceValues(Aero::Style& style) noexcept {
    const Aero::SetterBaseCollection setters = style.GetSetters();
    for (std::uint32_t index = 0U; index < setters.GetCount(); ++index) {
        if (SetterValueIsPending(
                ::Aero::TryCast<Aero::Setter>(setters.GetItem(index)))) {
            return true;
        }
    }
    const Aero::TriggerCollection triggers = style.GetTriggers();
    for (std::uint32_t index = 0U; index < triggers.GetCount(); ++index) {
        Aero::TriggerBase* authored = triggers.GetItem(index);
        if (authored == nullptr) {
            continue;
        }
        if (auto* setterTrigger = ::Aero::TryCast<Aero::SetterTriggerBase>(authored)) {
            if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
                auto* trigger = static_cast<Aero::Trigger*>(authored);
                if (!trigger->GetPropertyName().Empty() &&
                    trigger->GetAuthoredValue().IsUnset()) {
                    return true;
                }
            } else if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
                auto* trigger = static_cast<Aero::DataTrigger*>(authored);
                if (trigger->GetBinding() &&
                    trigger->GetAuthoredValue().IsUnset()) {
                    return true;
                }
            } else if (authored->RuntimeType() ==
                    Aero::MultiDataTrigger::StaticTypeId()) {
                auto* trigger =
                    static_cast<Aero::MultiDataTrigger*>(authored);
                for (const Base::Ref<Aero::Condition>& condition :
                     trigger->GetConditions()) {
                    if (condition &&
                        condition->GetBinding() &&
                        condition->GetAuthoredValue().IsUnset()) {
                        return true;
                    }
                }
            }
            for (const Base::Ref<Aero::Setter>& setterEntry :
                 setterTrigger->GetAuthoredSetters()) {
                if (SetterValueIsPending(setterEntry.Get())) {
                    return true;
                }
            }
            continue;
        }
    }
    return false;
}


Base::Result<void> XamlStyleSchemaFacet::FinalizeStyle(
    Aero::Style& style) noexcept {
    if (style.GetIsSealed()) {
        return {};
    }
    // Source-backed sibling dictionaries are committed after this EndInit
    // callback. A Setter.Value="{StaticResource ...}" whose key lives in a
    // theme/merged dictionary is queued as a deferred write; sealing now
    // would report "Style Setter requires Value". Leave the Style unsealed
    // until ObjectWriter reapplies EndInit after those writes.
    if (StyleHasPendingResourceValues(style)) {
        return {};
    }
    if (schema_ == nullptr || options_.properties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Style TargetType must be assigned before initialization completes");
    }
    if (style.GetTargetType() == Meta::InvalidTypeId) {
        if (!style.SetTargetType(
                Controls::Control::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style TargetType assignment failed");
        }
    }
    const Meta::TypeId targetType = style.GetTargetType();
    const Meta::TypeInfo* targetInfo =
        options_.properties->Types().FindType(targetType);
    if (targetInfo == nullptr ||
        HasTypeFlag(
            targetInfo->Flags(),
            Meta::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style TargetType must identify an object type");
    }
    auto resolveTriggerSetters = [&](
        Aero::SetterTriggerBase& trigger,
        const char* triggerKindName) -> Base::Result<void> {
        for (const Base::Ref<Aero::Setter>& setterEntry :
             trigger.GetAuthoredSetters()) {
            Aero::Setter* setter = setterEntry.Get();
            if (setter == nullptr || !setter->GetIsAuthored()) {
                thread_local char message[256]{};
                std::snprintf(
                    message,
                    sizeof(message),
                    "Style %s Setter requires Property and Value",
                    triggerKindName);
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    message);
            }
            const Meta::DependencyProperty* property =
                ResolveStyleProperty(
                    *options_.properties,
                    targetType,
                    setter->GetPropertyName());
            if (property == nullptr) {
                thread_local char message[256]{};
                std::snprintf(
                    message,
                    sizeof(message),
                    "%s Setter property",
                    triggerKindName);
                return MissingStyleProperty(
                    message,
                    setter->GetPropertyName(),
                    targetType,
                    options_.properties->Types());
            }
            Base::Result<Meta::PropertyValue> value =
                ConvertValueForProperty(
                    setter->GetAuthoredValue(),
                    targetType,
                    setter->GetPropertyName());
            if (!value) return value.GetStatus();
            if (!setter->Resolve(property->Handle(), value.Value())) {
                return InvalidStyle("Setter resolve value is invalid");
            }
            setter->ReleaseAuthored();
        }
        return {};
    };
    for (const Base::Ref<Aero::SetterBase>& entry :
         style.GetAuthoredSetters()) {
        Aero::Setter* setter =
            ::Aero::TryCast<Aero::Setter>(entry.Get());
        if (setter == nullptr) {
            continue;
        }
        if (setter->GetPropertyName().Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Setter requires Property");
        }
        if (setter->GetAuthoredValue().IsUnset()) {
            thread_local char message[256]{};
            const Base::StringView propertyName =
                setter->GetPropertyName();
            std::snprintf(
                message,
                sizeof(message),
                "Style Setter for '%.*s' requires Value",
                static_cast<int>(propertyName.SizeBytes()),
                propertyName.Data());
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                message);
        }
        const Meta::DependencyProperty* property =
            ResolveStyleProperty(
                *options_.properties,
                targetType,
                setter->GetPropertyName());
        if (property == nullptr) {
            return MissingStyleProperty(
                "Setter property",
                setter->GetPropertyName(),
                targetType,
                options_.properties->Types());
        }
        Base::Result<Meta::PropertyValue> value = ConvertValueForProperty(
            setter->GetAuthoredValue(),
            targetType,
            setter->GetPropertyName());
        if (!value) return value.GetStatus();
        if (!setter->Resolve(property->Handle(), value.Value())) {
            return InvalidStyle("Setter resolve value is invalid");
        }
        style.AddSetter(*setter);
        setter->ReleaseAuthored();
    }
    for (const Base::Ref<Aero::TriggerBase>& entry :
         style.GetAuthoredTriggers()) {
        Aero::TriggerBase* authored = entry.Get();
        if (authored == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style Trigger cannot be null");
        }
        if (authored->RuntimeType() == Aero::Trigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::Trigger*>(authored);
            if (!trigger->GetIsAuthored()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style Trigger requires Property and Value");
            }
            const Meta::DependencyProperty* condition =
                ResolveStyleProperty(
                    *options_.properties,
                    targetType,
                    trigger->GetPropertyName());
            if (condition == nullptr) {
                return MissingStyleProperty(
                    "Trigger property",
                    trigger->GetPropertyName(),
                    targetType,
                    options_.properties->Types());
            }
            Base::Result<Meta::PropertyValue> conditionValue =
                ConvertValueForProperty(
                    trigger->GetAuthoredValue(),
                    targetType,
                    trigger->GetPropertyName());
            if (!conditionValue) return conditionValue.GetStatus();
            trigger->SetProperty(condition->Handle());
            trigger->SetValue(conditionValue.Value());
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "Trigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            trigger->ReleaseAuthored();
            continue;
        }
        if (options_.properties->Types().IsDerivedFrom(
                authored->RuntimeType(),
                Media::Animation::EventTrigger::StaticTypeId())) {
            auto* trigger =
                static_cast<Media::Animation::EventTrigger*>(authored);
            if (trigger->GetRoutedEvent().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style EventTrigger requires RoutedEvent");
            }
            // Event triggers are immutable declarations. The View creates a
            // routed-event subscription per styled element after mounting.
            continue;
        }
        if (authored->RuntimeType() == Aero::DataTrigger::StaticTypeId()) {
            auto* trigger = static_cast<Aero::DataTrigger*>(authored);
            if (!trigger->GetBinding() ||
                trigger->GetAuthoredValue().IsUnset() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger requires Binding, Value, and Setters");
            }
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "DataTrigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            trigger->ReleaseAuthored();
            continue;
        }
        if (authored->RuntimeType() ==
                Aero::MultiDataTrigger::StaticTypeId()) {
            auto* trigger =
                static_cast<Aero::MultiDataTrigger*>(authored);
            if (trigger->GetConditions().Empty() ||
                trigger->GetAuthoredSetters().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style MultiDataTrigger requires Conditions and Setters");
            }
            for (const Base::Ref<Aero::Condition>& condition :
                 trigger->GetConditions()) {
                if (!condition || !condition->GetBinding() ||
                    condition->GetAuthoredValue().IsUnset()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Style MultiDataTrigger Condition requires Binding and Value");
                }
            }
            Base::Result<void> settersResolved = resolveTriggerSetters(
                *trigger, "MultiDataTrigger");
            if (!settersResolved) return settersResolved.GetStatus();
            style.AddTrigger(*trigger);
            trigger->ReleaseAuthored();
            continue;
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Style trigger type is not supported");
    }
    return Aero::SealStyle(
        style, *options_.properties);
}

Base::Result<void> XamlStyleSchemaFacet::EndStyleInit(
    Base::Object& object,
    void* context) noexcept {
    XamlStyleSchemaFacet* extension = static_cast<XamlStyleSchemaFacet*>(context);
    if (extension == nullptr) {
        return InvalidStyleXaml("Style initialization requires an extension context");
    }
    return extension->FinalizeStyle(
        static_cast<Aero::Style&>(
            object));
}

} // namespace Aero::Markup

namespace Aero::Markup {

struct UiObjectModel::State {
    explicit State(
        const UiObjectModelOptions& options) noexcept
        : style(options),
          templates(
              *options.metadata,
              *options.properties,
              options.allocator) {}

    XamlStyleSchemaFacet style;
    XamlTemplateSchemaFacet templates;
    bool registered = false;
};

UiObjectModel::UiObjectModel(
    const UiObjectModelOptions& options) noexcept
    : allocator_(options.allocator != nullptr
          ? options.allocator
          : &Base::GetDefaultAllocator()) {
    if (options.metadata == nullptr ||
        options.properties == nullptr) {
        return;
    }
    optionsValid_ = true;
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, options));
}

UiObjectModel::~UiObjectModel() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
    state_ = nullptr;
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema) noexcept {
    UiObjectModelTypes types;
    types.style = Aero::Style::StaticTypeId();
    types.setter = Aero::Setter::StaticTypeId();
    types.trigger = Aero::Trigger::StaticTypeId();
    types.styleProperty =
        Aero::FrameworkElement::StyleProperty;
    return Register(schema, types);
}

Base::Result<void> UiObjectModel::Register(
    Schema& schema,
    const UiObjectModelTypes& types) noexcept {
    if (!optionsValid_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML UI object model options are invalid");
    }
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML UI object model allocation failed");
    }
    if (state_->registered) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML UI object model is already registered");
    }
    Base::Result<void> registered =
        state_->style.Register(
            schema,
            types.style,
            types.setter,
            types.styleProperty,
            types.trigger);
    if (registered && types.includeTemplates) {
        registered = state_->templates.Register(schema);
    }
    if (registered) {
        state_->registered = true;
    }
    return registered;
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: TemplateSupport
// Source: src/gui/markup/TemplateSupport.cpp
// ============================================================================

// ===== TemplateSupport =====









namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Meta;
using namespace Aero::Threading;




Base::Status InvalidTemplateXaml(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

inline void ApplyTemplateBaseUriIfEmpty(
    FrameworkTemplate& tpl, const Base::ResourceUri* uri) noexcept {
    if (uri != nullptr && ::Aero::Controls::FrameworkTemplateState::BaseUri(tpl).Empty()) {
        ::Aero::Controls::FrameworkTemplateState::SetBaseUri(tpl, *uri);
    }
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        return &tpl->GetResources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Meta::TypeId key = Meta::InvalidTypeId;
    if (const auto* dt = TryCast<DataTemplate>(&object)) {
        key = dt->GetDataType();
    } else if (const auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        key = tpl->GetTargetType();
    }
    if (key == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

struct XamlTemplateSchemaFacet::State {
    State(
        Meta::Registry& metadata,
        DependencyPropertyRegistry& dependencyProperties,
        Base::IAllocator& programAllocator) noexcept
        : allocator(&programAllocator),
          runtime(&metadata),
          properties(&dependencyProperties) {}

    Base::IAllocator* allocator = nullptr;
    Meta::Registry* runtime = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    Schema* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const ExtensionServices& services,
        void* context) noexcept {
        auto* self = static_cast<XamlTemplateSchemaFacet::State*>(context);
        const TypeId type = object.RuntimeType();
        const bool isControlTemplate =
            type == ControlTemplate::StaticTypeId();
        const bool isDataTemplateFamily =
            type == DataTemplate::StaticTypeId() ||
            type == HierarchicalDataTemplate::StaticTypeId();
        const bool isItemsPanelTemplate =
            type == ItemsPanelTemplate::StaticTypeId();
        if (self == nullptr || self->runtime == nullptr ||
            self->properties == nullptr ||
            (!isControlTemplate &&
             !isDataTemplateFamily &&
             !isItemsPanelTemplate)) {
            return InvalidTemplateXaml(
                "Template deferred-content scope is invalid");
        }
        if (services.baseUri != nullptr) {
            if (isControlTemplate) {
                ApplyTemplateBaseUriIfEmpty(
                    static_cast<ControlTemplate&>(object), services.baseUri);
            } else if (isDataTemplateFamily) {
                ApplyTemplateBaseUriIfEmpty(
                    static_cast<DataTemplate&>(object), services.baseUri);
            } else {
                ApplyTemplateBaseUriIfEmpty(
                    static_cast<ItemsPanelTemplate&>(object), services.baseUri);
            }
        }
        return {};
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr ||
            (scopeOwner.RuntimeType() !=
                 ControlTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 DataTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 HierarchicalDataTemplate::StaticTypeId())) {
            return InvalidTemplateXaml(
                "Template name scope is invalid");
        }
        return ::Aero::Controls::FrameworkTemplateState::RegisterAuthoredName(
            static_cast<FrameworkTemplate&>(scopeOwner), name, object);
    }
};

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, runtime, properties, *allocator_));
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (state_ == nullptr) {
        return;
    }
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
    state_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        state_->schema != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML template extension registration is invalid");
    }

    state_->schema = &schema;
    Base::Result<void> status =
        schema.AddType({
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = schema.AddType({
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            HierarchicalDataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        state_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: GuiSchema
// Source: src/gui/markup/GuiSchema.cpp
// ============================================================================



namespace Aero {
namespace {

Base::Status InvalidBundleState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

struct GuiSchema::State {
    explicit State(Base::IAllocator& value) noexcept
        : allocator(&value) {}

    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry metadata;
    const ModuleSet* modules = nullptr;

    Markup::Schema* schema = nullptr;
    Markup::UiObjectModel* uiObjectModel = nullptr;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~State() noexcept {
        if (uiObjectModel != nullptr) {
            Base::FreeObject(*allocator, Base::MemoryTag::General, uiObjectModel);
            uiObjectModel = nullptr;
        }
        if (schema != nullptr) {
            Base::FreeObject(*allocator, Base::MemoryTag::General, schema);
            schema = nullptr;
        }
    }
};

GuiSchema::GuiSchema(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, *allocator_));
}

GuiSchema::~GuiSchema() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
}

Base::Result<void> GuiSchema::Prepare(
    const ModuleSet& modules) noexcept {
    if (state_ == nullptr || state_->terminal) {
        return InvalidBundleState("Schema bundle is terminal");
    }
    if (state_->prepared) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle metadata is already prepared");
    }
    Base::Result<void> status = modules.RegisterMetadata(state_->metadata);
    if (status) status = state_->metadata.Seal();
    if (!status) {
        state_->terminal = true;
        return status.GetStatus();
    }
    state_->prepared = true;
    state_->modules = &modules;
    return {};
}

Base::Result<void> GuiSchema::Finalize(
    const GuiSchemaOptions& requested) noexcept {
    if (state_ == nullptr || state_->terminal || !state_->prepared) {
        return InvalidBundleState(
            "Schema bundle metadata must be prepared before XAML finalization");
    }
    if (state_->frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle is already frozen");
    }
    Base::IAllocator& programAllocator = requested.allocator != nullptr
        ? *requested.allocator
        : *state_->allocator;

    Base::Result<void> status = Base::AllocateObject(
        *state_->allocator,
        Base::MemoryTag::General,
        state_->schema,
        state_->metadata,
        state_->allocator);
    if (!status) {
        state_->terminal = true;
        return status;
    }

    status = Markup::RegisterBuiltinMarkupExtensions(*state_->schema);
    if (status) {
        status = Base::AllocateObject(
            *state_->allocator,
            Base::MemoryTag::General,
            state_->uiObjectModel,
            Markup::UiObjectModelOptions{
                &state_->metadata,
                &(state_->metadata).DependencyProperties(),
                &programAllocator});
        if (status) {
            status = state_->uiObjectModel->Register(*state_->schema);
        }
    }
    if (status && state_->modules != nullptr) {
        status = state_->modules->RegisterResourceScopes(*state_->schema);
    }
    if (status) status = state_->metadata.Complete();
    if (status) status = state_->schema->Freeze();
    if (!status) {
        state_->terminal = true;
        return status.GetStatus();
    }
    state_->frozen = true;
    return {};
}

bool GuiSchema::IsPrepared() const noexcept {
    return state_ != nullptr && state_->prepared;
}

bool GuiSchema::IsFrozen() const noexcept {
    return state_ != nullptr && state_->frozen;
}

::Aero::Meta::Registry& GuiSchema::Metadata() noexcept {
    AERO_ASSERT(state_ != nullptr && state_->prepared);
    return state_->metadata;
}

const ::Aero::Meta::Registry& GuiSchema::Metadata() const noexcept {
    AERO_ASSERT(state_ != nullptr && state_->prepared);
    return state_->metadata;
}

Markup::Schema& GuiSchema::Schema() noexcept {
    AERO_ASSERT(state_ != nullptr && state_->schema != nullptr);
    return *state_->schema;
}

const Markup::Schema& GuiSchema::Schema() const noexcept {
    AERO_ASSERT(state_ != nullptr && state_->schema != nullptr);
    return *state_->schema;
}

} // namespace Aero

// ============================================================================
// SECTION: XamlSchemaConventions & Out-of-Line Runtime Contracts
// ============================================================================

namespace Aero::Markup {

ProvidedValue ProvidedValue::FromValue(
    Meta::Value&& provided) noexcept {
    ProvidedValue result;
    result.value = static_cast<Meta::Value&&>(provided);
    return result;
}

ProvidedValue ProvidedValue::Handled(
    void* context,
    std::uint64_t token,
    ProvidedRollbackCallback rollbackCallback) noexcept {
    ProvidedValue result;
    result.kind = ProvidedValueKind::Handled;
    result.rollbackContext = context;
    result.rollbackToken = token;
    result.rollback = rollbackCallback;
    return result;
}

ProvidedValue ProvidedValue::Expression(
    Meta::EffectiveValueEngine& engine,
    const Meta::PropertyExpression& provided) noexcept {
    ProvidedValue result;
    result.kind = ProvidedValueKind::Expression;
    result.effectiveValues = &engine;
    result.expression = provided;
    return result;
}

ProvidedValue ProvidedValue::Deferred(
    void* context,
    ProvidedCommitCallback commitCallback,
    ProvidedRollbackCallback rollbackCallback,
    ProvidedCleanupCallback cleanupCallback,
    ProvidedPrepareCallback prepareCallback,
    ProvidedBindCallback bindCallback) noexcept {
    ProvidedValue result;
    result.kind = ProvidedValueKind::Deferred;
    result.rollbackContext = context;
    result.prepare = prepareCallback;
    result.commit = commitCallback;
    result.rollback = rollbackCallback;
    result.cleanup = cleanupCallback;
    result.bind = bindCallback;
    return result;
}

void ProvidedValue::Discard() noexcept {
    if (kind == ProvidedValueKind::Expression &&
        expression.cleanup != nullptr) {
        expression.cleanup(expression.context);
    } else if (kind == ProvidedValueKind::Handled &&
               rollback != nullptr) {
        rollback(rollbackContext, rollbackToken);
    } else if (kind == ProvidedValueKind::Deferred &&
               cleanup != nullptr) {
        cleanup(rollbackContext);
    }
    expression = {};
    rollbackContext = nullptr;
    rollbackToken = 0U;
    rollback = nullptr;
    prepare = nullptr;
    commit = nullptr;
    cleanup = nullptr;
    bind = nullptr;
}

Base::Result<void> RegisterMarkupMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 4U;
    const Base::StringView name =
        MarkupMetadataModuleName();
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &PopulateMarkupMetadata,
        nullptr,
        nullptr});
}

Base::Result<void> CommittedEffect::Bind(
    const EffectServices& services) noexcept {
    if (!lifetime && services.lifetime) lifetime = services.lifetime;
    if (effectiveValues == nullptr && services.effectiveValues != nullptr) {
        effectiveValues = services.effectiveValues;
    }
    return bind != nullptr
        ? bind(context, services)
        : Base::Result<void>();
}

Base::Result<void> CommittedEffect::Prepare(
    const Aero::NameScope& names) noexcept {
    return prepare != nullptr
        ? prepare(context, names)
        : Base::Result<void>();
}

Base::Result<void> CommittedEffect::Commit() noexcept {
    if (committed) return {};
    if (lifetime && !lifetime->IsActive()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML effect runtime is no longer active");
    }
    if (pendingExpression.IsValid()) {
        if (effectiveValues == nullptr || target == nullptr ||
            !property.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Deferred XAML property expression is invalid");
        }
        Base::Result<void> installed =
            effectiveValues->SetLocalExpression(
                *target, property, pendingExpression);
        if (!installed) return installed.GetStatus();
        pendingExpression = {};
        committed = true;
        return {};
    }
    if (commit != nullptr) {
        Base::Result<std::uint64_t> result = commit(context);
        if (!result) return result.GetStatus();
        token = result.Value();
        committed = true;
        return {};
    }
    committed = true;
    return {};
}

void CommittedEffect::Rollback() noexcept {
    const bool runtimeActive = !lifetime || lifetime->IsActive();
    if (committed && runtimeActive) {
        if (effectiveValues != nullptr && target != nullptr) {
            static_cast<void>(effectiveValues->DetachObject(*target));
        }
        if (rollback != nullptr) {
            rollback(context, token);
        }
    } else if (!committed && pendingExpression.cleanup != nullptr) {
        pendingExpression.cleanup(pendingExpression.context);
    }
    if (cleanup != nullptr) cleanup(context);
    lifetime.Reset();
    effectiveValues = nullptr;
    target = nullptr;
    targetOwner.Reset();
    pendingExpression = {};
    context = nullptr;
    token = 0U;
    prepare = nullptr;
    commit = nullptr;
    rollback = nullptr;
    cleanup = nullptr;
    bind = nullptr;
    committed = false;
}

CommittedEffectPlan::~CommittedEffectPlan() noexcept {
    Rollback();
}

CommittedEffectPlan::CommittedEffectPlan(
    CommittedEffectPlan&& other) noexcept
    : effects_(std::move(other.effects_)) {}

CommittedEffectPlan& CommittedEffectPlan::operator=(
    CommittedEffectPlan&& other) noexcept {
    if (this == &other) return *this;
    Rollback();
    effects_ = std::move(other.effects_);
    return *this;
}

Base::Result<void> CommittedEffectPlan::Bind(
    const EffectServices& services) noexcept {
    for (CommittedEffect& effect : effects_) {
        Base::Result<void> bound = effect.Bind(services);
        if (!bound) return bound.GetStatus();
    }
    return {};
}

Base::Result<void> CommittedEffectPlan::Prepare(
    const Aero::NameScope& names) noexcept {
    for (CommittedEffect& effect : effects_) {
        Base::Result<void> prepared = effect.Prepare(names);
        if (!prepared) return prepared.GetStatus();
    }
    return {};
}

Base::Result<void> CommittedEffectPlan::Commit() noexcept {
    for (std::uint32_t index = 0U; index < effects_.Size(); ++index) {
        Base::Result<void> committed = effects_[index].Commit();
        if (committed) continue;
        for (std::uint32_t rollbackIndex = index;
             rollbackIndex > 0U; --rollbackIndex) {
            effects_[rollbackIndex - 1U].Rollback();
        }
        return committed.GetStatus();
    }
    return {};
}

void CommittedEffectPlan::Rollback() noexcept {
    for (std::uint32_t index = effects_.Size(); index > 0U; --index) {
        effects_[index - 1U].Rollback();
    }
    effects_.Clear();
}

void LoaderResult::Clear() noexcept {
    effects.Rollback();
    root.Reset();
    metadata = nullptr;
    names.Clear();
    resources = Aero::ResourceDictionary{};
    visualContent.ReleaseContent();
    visualContent.Clear();
    canonicalUri = {};
    dependencies.Clear();
    runtimeLifetime.Reset();
    hasDeferredStaticResources = false;
}

bool MatchesClrNamespacePrefix(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

bool IsExtensionsClrNamespace(Base::StringView value) noexcept {
    if (value.SizeBytes() < 31 ||
        !MatchesClrNamespacePrefix(value, Base::StringView("clr-namespace:"))) {
        return false;
    }
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
    if (value.SizeBytes() < 31) {
        return value;
    }
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
    switch (value.SizeBytes()) {
    case 8:
        if (value == Base::StringView("Geometry")) {
            return Base::StringView("StreamGeometry");
        }
        break;
    case 10:
        if (value == Base::StringView("WavesBrush")) {
            return Base::StringView("WavesShader");
        }
        break;
    case 15:
        if (value == Base::StringView("MonochromeBrush")) {
            return Base::StringView("MonochromeShader");
        }
        break;
    case 18:
        if (value == Base::StringView("ConicGradientBrush")) {
            return Base::StringView("ConicGradientShader");
        }
        break;
    case 21:
        if (value == Base::StringView("VisualStateTransition")) {
            return Base::StringView("VisualTransition");
        }
        break;
    default:
        break;
    }
    return value;
}

bool IsAeroExtensionsFacade(
    Base::StringView xamlNamespace,
    Base::StringView ownerName) noexcept {
    if (!IsExtensionsClrNamespace(xamlNamespace)) {
        return false;
    }
    switch (ownerName.SizeBytes()) {
    case 4:
        return ownerName == Base::StringView("Text") ||
               ownerName == Base::StringView("Path");
    case 5:
        return ownerName == Base::StringView("Brush");
    case 7:
        return ownerName == Base::StringView("Element");
    case 8:
        return ownerName == Base::StringView("RichText");
    default:
        return false;
    }
}

Base::Result<ParsedXamlMemberName> ParseXamlMemberName(
    Base::StringView localName,
    Base::StringView namespaceUri,
    Base::StringView targetXamlNamespace) noexcept {
    if (localName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member name cannot be empty");
    }
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U; index < localName.SizeBytes(); ++index) {
        if (localName[index] != '.') continue;
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML member contains multiple owner separators");
        }
        dot = index;
    }

    ParsedXamlMemberName result;
    if (dot == localName.SizeBytes()) {
        result.hasOwner = false;
        result.memberName = localName;
        return result;
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member owner syntax is invalid");
    }

    result.hasOwner = true;
    result.ownerName = localName.Substr(0U, dot);
    result.memberName = localName.Substr(dot + 1U, localName.SizeBytes() - dot - 1U);
    result.ownerNamespace = namespaceUri.Empty()
        ? targetXamlNamespace : namespaceUri;

    if (IsAeroExtensionsFacade(result.ownerNamespace, result.ownerName)) {
        if (result.ownerName == Base::StringView("Element") &&
            result.memberName == Base::StringView("BlendingMode")) {
            result.hasOwner = false;
            result.memberName = Base::StringView("BlendMode");
            result.isAliasedToBlendMode = true;
        }
    }

    return result;
}

ResolvedMember MakeResolvedPropertyMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId valueType,
    Meta::PropertyFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Property;
    resolved.ownerType = ownerType;
    resolved.valueType = valueType;
    resolved.propertyFlags = flags;
    resolved.attached = HasPropertyFlag(flags, Meta::PropertyFlags::Attached);
    return resolved;
}

ResolvedMember MakeResolvedEventMember(
    Meta::MemberId id,
    Meta::TypeId ownerType,
    Meta::TypeId eventArgsType,
    Meta::EventFlags flags) noexcept {
    ResolvedMember resolved;
    resolved.id = id;
    resolved.kind = Meta::MemberKind::Event;
    resolved.ownerType = ownerType;
    resolved.valueType = eventArgsType;
    resolved.eventFlags = flags;
    resolved.attached = HasEventFlag(flags, Meta::EventFlags::Attached);
    return resolved;
}

const ::Aero::Meta::DependencyProperty* ResolveXamlDependencyProperty(
    const ::Aero::Meta::DependencyPropertyRegistry& properties,
    ::Aero::Meta::TypeId targetType,
    ::Aero::Base::StringView name) noexcept {
    const ::Aero::Meta::DependencyProperty* property = properties.Find(targetType, name);
    if (property != nullptr) return property;

    if (name == ::Aero::Base::StringView("IsFocused") &&
        properties.Types().IsDerivedFrom(targetType, ::Aero::UIElement::StaticTypeId())) {
        return properties.Find(::Aero::UIElement::IsKeyboardFocusedProperty.Handle());
    }

    if (name == ::Aero::Base::StringView("ContextMenu")) {
        property = properties.Find(
            ::Aero::Controls::ContextMenuService::ContextMenuProperty.Handle());
        if (property != nullptr) return property;
    }

    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX || separator == 0U || separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }

    ::Aero::Base::StringView ownerPrefix;
    ::Aero::Base::StringView ownerName;
    SplitRelaxedQualifiedXamlName(name.Substr(0U, separator), ownerPrefix, ownerName);

    const ::Aero::Meta::TypeInfo* owner = properties.Types().FindType(
        ::Aero::Meta::AeroNamespaceUri(), ownerName);
    if (owner == nullptr && ownerName == ::Aero::Base::StringView("Text")) {
        owner = properties.Types().FindType(
            ::Aero::Meta::AeroNamespaceUri(), "TextProperties");
    }
    const ::Aero::Base::StringView member = name.Substr(
        separator + 1U, name.SizeBytes() - separator - 1U);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), member);
        if (property != nullptr) return property;
    }

    return properties.Find(targetType, member);
}

::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    ::Aero::Meta::TypeId targetPropertyType,
    const ::Aero::Meta::Value& value) noexcept {
    if (value.Type() == targetPropertyType) {
        return value;
    }

    if (targetPropertyType == ::Aero::Length::StaticTypeId() ||
        targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Length>() ||
        targetPropertyType == ::Aero::Meta::TypeOf<::Aero::GridLength>() ||
        targetPropertyType == ::Aero::Media::Brush::StaticTypeId() ||
        targetPropertyType == ::Aero::Meta::TypeOf<::Aero::Nullable<bool>>()) {
        auto converted = ::Aero::Base::ValueConversion::TryConvertDefaultValue(
            value, targetPropertyType);
        if (converted) return converted;
        if (targetPropertyType == ::Aero::Media::Brush::StaticTypeId() &&
            value.Type() == ::Aero::Meta::TypeOf<::Aero::Base::Color>()) {
            return converted;
        }
    }

    if (value.IsNullObject() && value.Type() != targetPropertyType) {
        return ::Aero::Meta::Value::NullObject(targetPropertyType);
    }

    return value;
}

::Aero::Base::Result<::Aero::Meta::Value> CoerceXamlValueForProperty(
    const ::Aero::Meta::DependencyProperty& property,
    const ::Aero::Meta::Value& value,
    const ::Aero::Meta::TypeRegistry& types) noexcept {
    if (property.ValueType() == ::Aero::Meta::TypeOf<::Aero::Base::Color>() &&
        value.Kind() == ::Aero::Meta::ValueKind::Object &&
        types.IsDerivedFrom(value.Type(), ::Aero::Media::Brush::StaticTypeId())) {
        return ::Aero::Base::ValueConversion::ConvertColorAndBrushValue(
            value, property.ValueType(), &types);
    }

    return CoerceXamlValueForProperty(property.ValueType(), value);
}

::Aero::Base::Result<::Aero::Meta::Value> BoxXamlStringAsObject(
    ::Aero::Base::StringView text) noexcept {
    Base::Result<Meta::Value> strVal =
        Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), text);
    if (!strVal) return strVal.GetStatus();
    Base::Result<Base::Ref<Controls::BoxedItemValue>> boxed =
        Base::MakeRef<Controls::BoxedItemValue>(std::move(strVal).Value());
    if (!boxed) return boxed.GetStatus();
    return Meta::Value::FromObject(
        Meta::TypeOf<Base::Object>(),
        Base::Ref<Base::Object>(std::move(boxed).Value()));
}

::Aero::Base::Status FormatMissingTargetPropertyStatus(
    const char* scopeName,
    const char* role,
    ::Aero::Base::StringView property,
    ::Aero::Meta::TypeId targetType,
    const ::Aero::Meta::TypeRegistry& types,
    ::Aero::Base::ErrorCode errorCode) noexcept {
    thread_local char message[384];
    const Meta::TypeInfo* target = types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr ? target->Name() : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "%s %s '%.*s' was not found on TargetType '%.*s'",
        scopeName,
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return Base::Status::Failure(errorCode, message);
}

MarkupExtensionContext MakeMarkupExtensionContext(
    const ExtensionServices& services) noexcept {
    auto typeResolver = [](void* ctx, Base::StringView name) noexcept -> Base::Result<const Meta::TypeInfo*> {
        const auto* s = static_cast<const ExtensionServices*>(ctx);
        if (s == nullptr || s->schema == nullptr || name.Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState, "ExtensionServices is null or name is empty");
        }
        Base::StringView prefix;
        Base::StringView localName;
        switch (SplitQualifiedXamlName(name, prefix, localName)) {
        case QualifiedNameSplitStatus::MultipleColons:
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Type reference contains multiple namespace prefixes");
        case QualifiedNameSplitStatus::MalformedPrefix:
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Type reference namespace prefix is malformed");
        case QualifiedNameSplitStatus::Ok:
            break;
        }
        Base::Result<Base::StringView> uri = s->namespaces.Lookup(prefix);
        if (!uri) return uri.GetStatus();
        return s->schema->ResolveType(uri.Value(), localName);
    };
    return MarkupExtensionContext(
        services.targetObject,
        services.targetMember,
        services.baseUri,
        const_cast<ExtensionServices*>(&services),
        typeResolver);
}

} // namespace Aero::Markup


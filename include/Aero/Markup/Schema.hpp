#pragma once

// Canonical public schema API.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/CompiledDocument.hpp>

#include <cstdint>

namespace Aero { class ResourceDictionary; class ResourceKey; }

namespace Aero {
class SchemaBundle;
}

namespace Aero::Markup {

struct ExtensionContext;
struct ProvidedValue;
class Loader;
class ObjectWriter;
class ObjectWriterState;
class SchemaManifest;

namespace Detail {
class SchemaAccess;
class XamlStyleSchemaFacet;

AERO_API Base::Result<void> PopulateMarkupMetadata(
    Core::MetadataContext& context) noexcept;
}

enum class MemberSyntax : std::uint8_t {
    Attribute = 0U,
    PropertyElement,
    Content
};

enum class MemberWriteMode : std::uint8_t {
    SetOnce = 0U,
    Collection
};

struct ResolvedMember final {
    Core::MemberId id = Core::InvalidMemberId;
    Core::MemberKind kind = Core::MemberKind::Property;
    Core::TypeId ownerType = Core::InvalidTypeId;
    Core::TypeId valueType = Core::InvalidTypeId;
    Core::PropertyFlags propertyFlags =
        Core::PropertyFlags::None;
    Core::EventFlags eventFlags = Core::EventFlags::None;
    bool attached = false;

    bool IsValid() const noexcept {
        return id != Core::InvalidMemberId &&
            ownerType != Core::InvalidTypeId &&
            valueType != Core::InvalidTypeId;
    }
};

struct MemberWritePolicy final {
    MemberWriteMode mode = MemberWriteMode::SetOnce;
    bool acceptsAnyValue = false;
    bool writable = false;
};

class AERO_API Schema final {
public:
    Schema(
        Core::MetadataDomain& domain,
        Core::MetadataRuntime& runtime,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Schema() noexcept;

    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    bool IsFrozen() const noexcept { return frozen_; }
    const Core::TypeRegistry& Types() const noexcept {
        return runtime_->Types();
    }
    Base::Result<const Core::TypeInfo*> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<ResolvedMember> ResolveMember(
        Core::TypeId targetType,
        const QualifiedName& name,
        MemberSyntax syntax) const noexcept;
    Base::Result<ResolvedMember> ResolveContentMember(
        Core::TypeId targetType) const noexcept;

private:
    friend class ::Aero::SchemaBundle;
    friend class CompiledDocument;
    friend class Detail::SchemaAccess;
    friend class Loader;
    friend class ObjectWriter;
    friend class ObjectWriterState;
    friend class SchemaManifest;
    friend class Detail::XamlStyleSchemaFacet;

    Base::Result<void> Freeze() noexcept;
    bool UsesRuntime() const noexcept { return runtime_ != nullptr; }
    Core::MetadataRuntime* Runtime() const noexcept { return runtime_; }
    const Core::MetadataDomain& Domain() const noexcept {
        return *domain_;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;
    Base::Result<Core::DependencyObject*> ResolvePropertyTarget(
        Base::Object& object) const noexcept;
    Base::Result<Core::Value> ConvertText(
        Core::TypeId type,
        Base::StringView text,
        const ExtensionContext* services = nullptr) const noexcept;
    Base::Result<void> SetMember(
        Base::Object& object,
        Core::TypeId objectType,
        const ResolvedMember& member,
        const Core::Value& value) const noexcept;
    Base::Result<ProvidedValue> ProvideMarkupExtensionValue(
        Core::TypeId type,
        Base::StringView arguments,
        const ExtensionContext& services) const noexcept;

    Base::Result<void> BeginInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    Base::Result<void> EndInit(
        Core::TypeId type,
        Base::Object& object,
        const ExtensionContext& services) const noexcept;
    void AbortInit(Core::TypeId type, Base::Object& object) const noexcept;

    bool CreatesNameScope(Core::TypeId type) const noexcept;
    bool CreatesResourceScope(Core::TypeId type) const noexcept;
    bool DefersVisualContent(Core::TypeId type) const noexcept;
    Base::Result<void> RegisterName(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object) const noexcept;
    Base::Result<void> AddResource(
        Core::TypeId scopeType,
        Base::Object& scopeOwner,
        const Aero::ResourceKey& key,
        const Core::Value& value) const noexcept;
    Aero::ResourceDictionary* ResolveResourceScope(
        Core::TypeId scopeType,
        Base::Object& scopeOwner) const noexcept;
    Base::Result<Aero::ResourceKey> ResolveImplicitResourceKey(
        Core::TypeId type,
        const Base::Object& object) const noexcept;

    MemberWritePolicy ResolveMemberWritePolicy(
        const ResolvedMember& member) const noexcept;

    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    Core::MetadataDomain* domain_ = nullptr;
    Core::MetadataRuntime* runtime_ = nullptr;
    bool frozen_ = false;

    Base::Result<ResolvedMember> ResolvePropertyOrEventRuntime(
        Core::TypeId targetType,
        Core::TypeId ownerType,
        Base::StringView memberName,
        MemberSyntax syntax,
        bool ownerWasExplicit) const noexcept;
};

struct SchemaManifestLimits final {
    std::uint32_t maxTypes = 100000U;
    std::uint32_t maxMembers = 500000U;
    std::uint32_t maxStringBytes =
        64U * 1024U * 1024U;
};

struct SchemaTypeInfo final {
    Core::TypeId id = Core::InvalidTypeId;
    Core::MetadataTypeKind kind =
        Core::MetadataTypeKind::Object;
    Core::TypeFlags flags = Core::TypeFlags::None;
};

class AERO_API SchemaManifest final {
public:
    struct Impl;
    explicit SchemaManifest(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~SchemaManifest() noexcept;

    SchemaManifest(SchemaManifest&& other) noexcept;
    SchemaManifest& operator=(
        SchemaManifest&& other) noexcept;

    SchemaManifest(const SchemaManifest&) = delete;
    SchemaManifest& operator=(const SchemaManifest&) = delete;

    static Base::Result<SchemaManifest> Capture(
        const Schema& schema,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<SchemaManifest> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const SchemaManifestLimits& limits = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<Base::Vector<std::uint8_t>>
    Serialize() const noexcept;

    bool IsValid() const noexcept;
    std::uint32_t TypeCount() const noexcept;
    std::uint32_t MemberCount() const noexcept;

    const CompiledCacheIdentity& Identity() const noexcept;

    Base::Result<SchemaTypeInfo> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<ResolvedMember> ResolveMember(
        Core::TypeId targetType,
        const QualifiedName& name,
        MemberSyntax syntax) const noexcept;
    Base::Result<ResolvedMember> ResolveContentMember(
        Core::TypeId targetType) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;

    explicit SchemaManifest(
        Base::IAllocator& allocator,
        Impl* impl) noexcept;
};

inline constexpr Base::StringView
MarkupMetadataModuleName() noexcept {
    return "Aero.Markup";
}

inline Base::Result<void> TryRegisterMarkupMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 4U;
    const Base::StringView name =
        MarkupMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateMarkupMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Markup

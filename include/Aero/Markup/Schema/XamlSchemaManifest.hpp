#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Version.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDescriptors.hpp>
#include <Aero/Markup/Compiled/XamlCompiledCache.hpp>
#include <Aero/Markup/Parsing/XamlNode.hpp>
#include <Aero/Markup/Schema/XamlResolvedMember.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlSchemaContext;


struct XamlSchemaManifestLimits final {
    std::uint32_t maxTypes = 100000U;
    std::uint32_t maxMembers = 500000U;
    std::uint32_t maxStringBytes = 64U * 1024U * 1024U;
};

struct XamlSchemaTypeInfo final {
    Core::TypeId id = Core::InvalidTypeId;
    Core::MetadataTypeKind kind = Core::MetadataTypeKind::Object;
    Core::TypeFlags flags = Core::TypeFlags::None;
};

// Host-tool snapshot of the immutable XAML validation surface. The manifest
// contains only stable identifiers and descriptor data; runtime callbacks,
// factories, allocators, and target-platform implementation details are never
// serialized. It is therefore safe to consume from a host aero-xamlc while the
// target runtime is being cross-compiled.
class AERO_API XamlSchemaManifest final {
public:
    struct Impl;
    explicit XamlSchemaManifest(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlSchemaManifest() noexcept;

    XamlSchemaManifest(XamlSchemaManifest&& other) noexcept;
    XamlSchemaManifest& operator=(XamlSchemaManifest&& other) noexcept;

    XamlSchemaManifest(const XamlSchemaManifest&) = delete;
    XamlSchemaManifest& operator=(const XamlSchemaManifest&) = delete;

    static Base::Result<XamlSchemaManifest> Capture(
        const XamlSchemaContext& schema,
        Base::IAllocator* allocator = nullptr) noexcept;
    static Base::Result<XamlSchemaManifest> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const XamlSchemaManifestLimits& limits = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<Base::Vector<std::uint8_t>> Serialize() const noexcept;

    bool IsValid() const noexcept;
    std::uint32_t TypeCount() const noexcept;
    std::uint32_t MemberCount() const noexcept;

    const XamlCompiledCacheIdentity& Identity() const noexcept;

    Base::Result<XamlSchemaTypeInfo> ResolveType(
        Base::StringView xamlNamespace,
        Base::StringView localName) const noexcept;
    Base::Result<XamlResolvedMember> ResolveMember(
        Core::TypeId targetType,
        const XamlQualifiedName& name,
        XamlMemberSyntax syntax) const noexcept;
    Base::Result<XamlResolvedMember> ResolveContentMember(
        Core::TypeId targetType) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;

    explicit XamlSchemaManifest(
        Base::IAllocator& allocator,
        Impl* impl) noexcept;
};

} // namespace Aero::Markup

#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Markup/Compiled/XamlCompiledCache.hpp>
#include <Aero/Markup/Parsing/XamlNode.hpp>

namespace Aero::Markup {

class XamlNodeReader;
class XamlSchemaContext;

struct XamlCompiledDocumentLimits final {
    std::uint32_t maxNodes = 100000U;
    std::uint32_t maxStringBytes = 16U * 1024U * 1024U;
    std::uint32_t maxDependencies = 4096U;
};

// Immutable replay IR produced from the XML node stream. It removes XML
// tokenization from the load path and is guarded by the same metadata schema
// identity used by persisted compiled-XAML caches.
class AERO_API XamlCompiledDocument final {
public:
    XamlCompiledDocument() noexcept = default;

    XamlCompiledDocument(const XamlCompiledDocument&) = delete;
    XamlCompiledDocument& operator=(const XamlCompiledDocument&) = delete;
    XamlCompiledDocument(XamlCompiledDocument&&) noexcept = default;
    XamlCompiledDocument& operator=(
        XamlCompiledDocument&&) noexcept = default;

    static Base::Result<XamlCompiledDocument> Compile(
        XamlNodeReader& reader,
        const Core::MetadataDomain& domain) noexcept;
    static Base::Result<XamlCompiledDocument> Compile(
        XamlNodeReader& reader,
        const Core::MetadataDomain& domain,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<XamlCompiledDocument> Compile(
        XamlNodeReader& reader,
        const XamlSchemaContext& schema) noexcept;
    static Base::Result<XamlCompiledDocument> Compile(
        XamlNodeReader& reader,
        const XamlSchemaContext& schema,
        const Base::ResourceUri& originUri) noexcept;
    Base::Result<void> ValidateSchema(
        const XamlSchemaContext& schema) const noexcept;
    Base::Result<Base::Vector<std::uint8_t>>
    Serialize() const noexcept;
    static Base::Result<XamlCompiledDocument> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const Core::MetadataDomain& domain,
        const XamlCompiledDocumentLimits& limits = {}) noexcept;

    const XamlCompiledCacheIdentity& Identity() const noexcept {
        return identity_;
    }
    Base::Span<const XamlNode> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    const Base::ResourceUri& OriginUri() const noexcept {
        return originUri_;
    }
    Base::Span<const Base::ResourceUri> Dependencies() const noexcept {
        return {dependencies_.Data(), dependencies_.Size()};
    }
    Base::Result<void> TryAddDependency(
        const Base::ResourceUri& dependency) noexcept;
    bool IsValid() const noexcept {
        return !nodes_.Empty() &&
            nodes_.Back().Kind() ==
                XamlNodeKind::EndOfDocument;
    }

private:
    XamlCompiledCacheIdentity identity_;
    Base::ResourceUri originUri_;
    Base::Vector<Base::ResourceUri> dependencies_;
    Base::Vector<XamlNode> nodes_;
};

} // namespace Aero::Markup

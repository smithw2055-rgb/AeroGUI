#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Markup/XamlCompiledCache.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>

namespace Aero::Markup {

class XamlSchemaContext;

struct XamlCompiledDocumentLimits final {
    std::uint32_t maxNodes = 100000U;
    std::uint32_t maxStringBytes = 16U * 1024U * 1024U;
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
        const XamlSchemaContext& schema) noexcept;
    Base::Result<void> ValidateSchema(
        const XamlSchemaContext& schema) const noexcept;
    Base::Result<Base::Vector<std::uint8_t>>
    Serialize() const noexcept;
    static Base::Result<XamlCompiledDocument> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const Core::MetadataDomain& domain,
        const XamlCompiledDocumentLimits& limits = {},
        Base::HashCode moduleManifestHash = 0U) noexcept;

    const XamlCompiledCacheIdentity& Identity() const noexcept {
        return identity_;
    }
    Base::Span<const XamlNode> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    bool IsValid() const noexcept {
        return !nodes_.Empty() &&
            nodes_.Back().Kind() ==
                XamlNodeKind::EndOfDocument;
    }

private:
    XamlCompiledCacheIdentity identity_;
    Base::Vector<XamlNode> nodes_;
};

} // namespace Aero::Markup

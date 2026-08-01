#pragma once

// Canonical compiled-document API.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Meta/Registry.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero::Markup {

class Schema;
class SchemaManifest;
class CompiledDocument;
class NodeReader;

enum class XmlTokenKind : std::uint8_t {
    None = 0U,
    StartElement,
    EndElement,
    Text,
    EndOfDocument
};

struct XmlTokenizerLimits final {
    std::uint64_t maxInputBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxDepth = 256U;
    std::uint32_t maxAttributesPerElement = 256U;
    std::uint32_t maxNameBytes = 1024U;
    std::uint32_t maxTextBytes = 1024U * 1024U;
};

namespace XmlDiagnosticCodes {
inline constexpr Core::DiagnosticCode InvalidUtf8 =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 1U);
inline constexpr Core::DiagnosticCode InputLimitExceeded =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 2U);
inline constexpr Core::DiagnosticCode UnexpectedEndOfInput =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 3U);
inline constexpr Core::DiagnosticCode MalformedMarkup =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 4U);
inline constexpr Core::DiagnosticCode UnsupportedDeclaration =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 5U);
inline constexpr Core::DiagnosticCode DepthLimitExceeded =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 6U);
inline constexpr Core::DiagnosticCode AttributeLimitExceeded =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 7U);
inline constexpr Core::DiagnosticCode NameLimitExceeded =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 8U);
inline constexpr Core::DiagnosticCode TextLimitExceeded =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 9U);
inline constexpr Core::DiagnosticCode MismatchedEndElement =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 10U);
inline constexpr Core::DiagnosticCode MultipleDocumentElements =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 11U);
inline constexpr Core::DiagnosticCode UnknownEntity =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 12U);
inline constexpr Core::DiagnosticCode InvalidXmlCharacter =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 13U);
inline constexpr Core::DiagnosticCode DuplicateAttribute =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 14U);
} // namespace XmlDiagnosticCodes

class AERO_API XmlAttribute final {
public:
    XmlAttribute() noexcept = default;
    XmlAttribute(XmlAttribute&&) noexcept = default;
    XmlAttribute& operator=(XmlAttribute&&) noexcept = default;

    XmlAttribute(const XmlAttribute&) = delete;
    XmlAttribute& operator=(const XmlAttribute&) = delete;

    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    Core::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    Core::SourceSpan ValueSource() const noexcept {
        return valueSource_;
    }

private:
    friend class Utf8XmlTokenizer;
    friend class ExpatXmlTokenizer;

    Base::String name_;
    Base::String value_;
    Core::SourceSpan source_;
    Core::SourceSpan nameSource_;
    Core::SourceSpan valueSource_;
};

class AERO_API XmlToken final {
public:
    XmlToken() noexcept = default;
    XmlToken(XmlToken&&) noexcept = default;
    XmlToken& operator=(XmlToken&&) noexcept = default;

    XmlToken(const XmlToken&) = delete;
    XmlToken& operator=(const XmlToken&) = delete;

    void Clear() noexcept;

    XmlTokenKind Kind() const noexcept { return kind_; }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Text() const noexcept {
        return text_.View();
    }
    Base::Span<const XmlAttribute> Attributes() const noexcept {
        return {attributes_.Data(), attributes_.Size()};
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    Core::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    bool IsEmptyElement() const noexcept {
        return emptyElement_;
    }

private:
    friend class Utf8XmlTokenizer;
    friend class ExpatXmlTokenizer;

    XmlTokenKind kind_ = XmlTokenKind::None;
    Base::String name_;
    Base::String text_;
    Base::Vector<XmlAttribute> attributes_;
    Core::SourceSpan source_;
    Core::SourceSpan nameSource_;
    bool emptyElement_ = false;
};

class AERO_API IXmlTokenizer {
public:
    virtual ~IXmlTokenizer() = default;

    virtual Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept = 0;
    virtual Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept = 0;
    virtual std::uint32_t Depth() const noexcept = 0;
};

class AERO_API Utf8XmlTokenizer final
    : public IXmlTokenizer {
public:
    explicit Utf8XmlTokenizer(
        XmlTokenizerLimits limits = {}) noexcept;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return openElements_.Size();
    }

    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }
    std::uint64_t ByteOffset() const noexcept {
        return offset_;
    }

private:
    XmlTokenizerLimits limits_;
    Base::StringView input_;
    Base::Vector<Base::String> openElements_;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    std::uint32_t offset_ = 0U;
    std::uint32_t line_ = 1U;
    std::uint32_t column_ = 1U;
    bool rootSeen_ = false;
    bool rootClosed_ = false;
    bool endEmitted_ = false;
    bool initialized_ = false;
    bool failed_ = false;

    bool AtEnd() const noexcept;
    bool StartsWith(
        const char* literal,
        std::uint32_t length) const noexcept;
    Core::SourcePosition Position() const noexcept;
    Core::SourceSpan SpanFrom(
        Core::SourcePosition begin) const noexcept;
    std::uint32_t CurrentCodePointLength() const noexcept;
    std::uint32_t CurrentCodePoint() const noexcept;
    void AdvanceCodePoint() noexcept;
    void ConsumeAscii(std::uint32_t count) noexcept;
    bool SkipWhitespace() noexcept;

    Base::Result<XmlTokenKind> ParseStartElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseEndElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseText(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseCdata(
        XmlToken& token) noexcept;
    Base::Result<void> ParseName(
        Base::String& name,
        Core::SourceSpan& source) noexcept;
    Base::Result<void> ParseAttributeValue(
        char quote,
        Base::String& value,
        Core::SourceSpan& source) noexcept;
    Base::Result<void> AppendEntity(
        Base::String& output) noexcept;
    Base::Result<void> AppendCodePoint(
        Base::String& output,
        std::uint32_t codePoint) noexcept;
    Base::Result<void> AppendCurrentCodePoint(
        Base::String& output,
        bool attributeValue = false) noexcept;
    Base::Result<void> SkipComment() noexcept;
    Base::Result<void> SkipProcessingInstruction() noexcept;

    Base::Status Failure(
        Base::ErrorCode error,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

class AERO_API ExpatXmlTokenizer final
    : public IXmlTokenizer {
public:
    explicit ExpatXmlTokenizer(
        XmlTokenizerLimits limits = {}) noexcept;
    ~ExpatXmlTokenizer() noexcept override;

    ExpatXmlTokenizer(const ExpatXmlTokenizer&) = delete;
    ExpatXmlTokenizer& operator=(
        const ExpatXmlTokenizer&) = delete;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return depth_;
    }
    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }

private:
    XmlTokenizerLimits limits_;
    Base::Vector<XmlToken> tokens_;
    Base::Vector<std::uint32_t> tokenDepths_;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    void* parser_ = nullptr;
    Base::Status failure_;
    std::uint32_t readIndex_ = 0U;
    std::uint32_t parseDepth_ = 0U;
    std::uint32_t depth_ = 0U;
    bool initialized_ = false;

    void HandleStart(
        const char* name,
        const char** attributes) noexcept;
    void HandleEnd(const char* name) noexcept;
    void HandleText(
        const char* text,
        int length) noexcept;
    void RejectDeclaration() noexcept;
    int RejectExternalEntity() noexcept;
    Core::SourcePosition Position() const noexcept;
    void Stop(
        Base::Status status,
        Core::DiagnosticCode diagnostic,
        Base::StringView message) noexcept;
    Base::Result<void> PushToken(
        XmlToken&& token,
        std::uint32_t depth) noexcept;
};

enum class NodeKind : std::uint8_t {
    None = 0U,
    NamespaceDeclaration,
    StartObject,
    EndObject,
    StartMember,
    EndMember,
    Value,
    EndOfDocument
};

namespace NodeDiagnosticCodes {
inline constexpr Core::DiagnosticCode UnboundNamespacePrefix =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 101U);
inline constexpr Core::DiagnosticCode InvalidNamespaceDeclaration =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 102U);
inline constexpr Core::DiagnosticCode DuplicateNamespacePrefix =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 103U);
inline constexpr Core::DiagnosticCode InvalidNodeStreamState =
    Core::MakeDiagnosticCode(
        Core::DiagnosticDomain::Xaml, 104U);
} // namespace NodeDiagnosticCodes

class AERO_API QualifiedName final {
public:
    QualifiedName() noexcept = default;
    QualifiedName(QualifiedName&&) noexcept = default;
    QualifiedName& operator=(QualifiedName&&) noexcept = default;

    QualifiedName(const QualifiedName&) = delete;
    QualifiedName& operator=(const QualifiedName&) = delete;

    Base::StringView Prefix() const noexcept {
        return prefix_.View();
    }
    Base::StringView LocalName() const noexcept {
        return localName_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    bool HasNamespace() const noexcept {
        return !namespaceUri_.Empty();
    }

private:
    friend class Node;
    friend class NodeReader;
    friend class CompiledDocument;

    void Clear() noexcept;

    Base::String prefix_;
    Base::String localName_;
    Base::String namespaceUri_;
};

class AERO_API Node final {
public:
    Node() noexcept = default;
    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    void Clear() noexcept;
    static Base::Result<Node> TryClone(
        const Node& source) noexcept;

    NodeKind Kind() const noexcept { return kind_; }
    const QualifiedName& Name() const noexcept {
        return name_;
    }
    Base::StringView NamespacePrefix() const noexcept {
        return namespacePrefix_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    bool IsFromAttribute() const noexcept {
        return fromAttribute_;
    }

private:
    friend class NodeReader;
    friend class CompiledDocument;

    NodeKind kind_ = NodeKind::None;
    QualifiedName name_;
    Base::String namespacePrefix_;
    Base::String namespaceUri_;
    Base::String value_;
    Core::SourceSpan source_;
    bool fromAttribute_ = false;
};

class AERO_API NodeReader final {
public:
    explicit NodeReader(
        IXmlTokenizer& tokenizer,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    void Reset() noexcept;
    Base::Result<NodeKind> Read(
        Node& node) noexcept;
    std::uint32_t ObjectDepth() const noexcept {
        return scopes_.Size();
    }

private:
    struct NamespaceBinding final {
        NamespaceBinding() noexcept = default;
        NamespaceBinding(NamespaceBinding&&) noexcept = default;
        NamespaceBinding& operator=(NamespaceBinding&&) noexcept = default;
        NamespaceBinding(const NamespaceBinding&) = delete;
        NamespaceBinding& operator=(const NamespaceBinding&) = delete;

        Base::String prefix;
        Base::String uri;
    };

    struct ScopeFrame final {
        ScopeFrame() noexcept = default;
        ScopeFrame(ScopeFrame&&) noexcept = default;
        ScopeFrame& operator=(ScopeFrame&&) noexcept = default;
        ScopeFrame(const ScopeFrame&) = delete;
        ScopeFrame& operator=(const ScopeFrame&) = delete;

        std::uint32_t bindingStart = 0U;
        std::uint32_t ignorableNamespaceStart = 0U;
        QualifiedName objectName;
        bool ignored = false;
    };

    IXmlTokenizer* tokenizer_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    XmlToken xmlToken_;
    Base::Vector<Node> pending_;
    Base::Vector<NamespaceBinding> bindings_;
    Base::Vector<Base::String> ignorableNamespaces_;
    Base::Vector<ScopeFrame> scopes_;
    std::uint32_t pendingIndex_ = 0U;
    bool ended_ = false;
    bool failed_ = false;

    Base::Result<void> QueueStartElement(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueEndElement(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueText(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueEndOfDocument(
        const XmlToken& token) noexcept;
    Base::Result<void> QueueNamespaceDeclaration(
        Base::StringView prefix,
        Base::StringView uri,
        Core::SourceSpan source) noexcept;
    Base::Result<void> QueueObjectNode(
        NodeKind kind,
        const QualifiedName& name,
        Core::SourceSpan source) noexcept;
    Base::Result<void> QueueMemberNodes(
        const XmlAttribute& attribute) noexcept;
    Base::Result<void> AppendPending(
        Node&& node) noexcept;

    Base::Result<void> AddNamespaceBinding(
        Base::StringView prefix,
        Base::StringView uri,
        std::uint32_t bindingStart,
        Core::SourceSpan source) noexcept;
    Base::Result<void> AddIgnorableNamespaces(
        Base::StringView prefixes,
        Core::SourceSpan source) noexcept;
    Base::Result<void> ResolveQualifiedName(
        Base::StringView qualifiedName,
        bool useDefaultNamespace,
        Core::SourceSpan source,
        QualifiedName& output) noexcept;
    Base::Result<void> CopyQualifiedName(
        const QualifiedName& source,
        QualifiedName& output) noexcept;
    Base::StringView LookupNamespace(
        Base::StringView prefix,
        bool& found) const noexcept;
    bool IsNamespaceDeclaration(
        Base::StringView attributeName,
        Base::StringView& prefix) const noexcept;
    bool IsIgnorableNamespace(
        Base::StringView uri) const noexcept;
    bool IsMarkupCompatibilityIgnorable(
        const XmlAttribute& attribute) const noexcept;
    void PopBindings(std::uint32_t bindingStart) noexcept;
    void PopIgnorableNamespaces(std::uint32_t start) noexcept;

    Base::Result<NodeKind> EmitPending(
        Node& node) noexcept;
    Base::Status Failure(
        Base::ErrorCode error,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

struct CompiledCacheIdentity final {
    std::uint32_t cacheFormatVersion =
        XamlCompiledCacheFormatVersion;
    std::uint32_t typeIdAlgorithmVersion =
        Core::TypeIdAlgorithmVersion;
    std::uint32_t metadataSchemaFormatVersion =
        Core::MetadataSchemaFormatVersion;
    std::uint32_t metadataProgramFormatVersion =
        Core::MetadataProgramFormatVersion;
    std::uint32_t schemaVersion =
        XamlSchemaAbiVersion;
    Base::HashCode metadataSchemaHash = 0U;
};

enum class CompiledCacheCompatibility : std::uint8_t {
    Compatible = 0U,
    CacheFormatMismatch,
    TypeIdAlgorithmMismatch,
    MetadataSchemaFormatMismatch,
    MetadataProgramFormatMismatch,
    SchemaVersionMismatch,
    MetadataSchemaMismatch
};

AERO_API Base::Result<CompiledCacheIdentity>
BuildCompiledCacheIdentity(
    const ::Aero::Meta::Registry& domain) noexcept;

AERO_API CompiledCacheCompatibility
CompareCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const CompiledCacheIdentity& current) noexcept;

AERO_API Base::Result<void> ValidateCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const ::Aero::Meta::Registry& currentDomain) noexcept;

struct CompiledDocumentLimits final {
    std::uint32_t maxNodes = 100000U;
    std::uint32_t maxStringBytes = 16U * 1024U * 1024U;
    std::uint32_t maxDependencies = 4096U;
};

// Immutable replay IR produced from the XML node stream. It removes XML
// tokenization from the load path and is guarded by the same metadata schema
// identity used by persisted compiled-XAML caches.
class AERO_API CompiledDocument final {
public:
    CompiledDocument() noexcept = default;

    CompiledDocument(const CompiledDocument&) = delete;
    CompiledDocument& operator=(const CompiledDocument&) = delete;
    CompiledDocument(CompiledDocument&&) noexcept = default;
    CompiledDocument& operator=(
        CompiledDocument&&) noexcept = default;

    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const ::Aero::Meta::Registry& domain) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const ::Aero::Meta::Registry& domain,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const Schema& schema) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const Schema& schema,
        const Base::ResourceUri& originUri) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const SchemaManifest& manifest) noexcept;
    static Base::Result<CompiledDocument> Compile(
        NodeReader& reader,
        const SchemaManifest& manifest,
        const Base::ResourceUri& originUri) noexcept;
    Base::Result<void> ValidateSchema(
        const Schema& schema) const noexcept;
    Base::Result<void> ValidateSchema(
        const SchemaManifest& manifest) const noexcept;
    Base::Result<Base::Vector<std::uint8_t>>
    Serialize() const noexcept;
    static Base::Result<CompiledDocument> Deserialize(
        Base::Span<const std::uint8_t> bytes,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept;

    const CompiledCacheIdentity& Identity() const noexcept {
        return identity_;
    }
    Base::Span<const Node> Nodes() const noexcept {
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
                NodeKind::EndOfDocument;
    }

private:
    static Base::Result<CompiledDocument> CompileWithIdentity(
        NodeReader& reader,
        const CompiledCacheIdentity& identity,
        const Base::ResourceUri& originUri) noexcept;

    CompiledCacheIdentity identity_;
    Base::ResourceUri originUri_;
    Base::Vector<Base::ResourceUri> dependencies_;
    Base::Vector<Node> nodes_;
};

} // namespace Aero::Markup

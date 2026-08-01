#pragma once

#include <Aero/Markup.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Version.hpp>
#include "gui/MetadataInternal.hpp"

// Canonical compiled-document API.

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

// Canonical public schema API.

namespace Aero { class DependencyObject; class ResourceDictionary; class ResourceKey; }

namespace Aero {
class GuiSchema;
}

namespace Aero::Markup {

struct ExtensionServices;
struct ProvidedValue;
class Loader;
class ObjectWriter;
class ObjectBuilder;
class SchemaManifest;

namespace Detail {
class SchemaPrivate;
class XamlStyleSchemaFacet;

AERO_API Base::Result<void> PopulateMarkupMetadata(
    ::Aero::Meta::Registration& context) noexcept;
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
        ::Aero::Meta::Registry& metadata,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Schema() noexcept;

    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    bool IsFrozen() const noexcept { return frozen_; }
    const Core::TypeRegistry& Types() const noexcept {
        return domain_->Types();
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
    friend class ::Aero::GuiSchema;
    friend class CompiledDocument;
    friend class Detail::SchemaPrivate;
    friend class Loader;
    friend class ObjectWriter;
    friend class ObjectBuilder;
    friend class SchemaManifest;
    friend class Detail::XamlStyleSchemaFacet;

    Base::Result<void> Freeze() noexcept;
    ::Aero::Meta::Registry* Metadata() const noexcept { return domain_; }
    const ::Aero::Meta::Registry& Domain() const noexcept {
        return *domain_;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId type) const noexcept;
    Base::Result<::Aero::DependencyObject*> ResolvePropertyTarget(
        Base::Object& object) const noexcept;
    Base::Result<Core::Value> ConvertText(
        Core::TypeId type,
        Base::StringView text,
        const ExtensionServices* services = nullptr) const noexcept;
    Base::Result<void> SetMember(
        Base::Object& object,
        Core::TypeId objectType,
        const ResolvedMember& member,
        const Core::Value& value) const noexcept;
    Base::Result<ProvidedValue> ProvideMarkupExtensionValue(
        Core::TypeId type,
        Base::StringView arguments,
        const ExtensionServices& services) const noexcept;

    Base::Result<void> BeginInit(
        Core::TypeId type,
        Base::Object& object) const noexcept;
    Base::Result<void> EndInit(
        Core::TypeId type,
        Base::Object& object,
        const ExtensionServices& services) const noexcept;
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
    ::Aero::Meta::Registry* domain_ = nullptr;
    bool frozen_ = false;

    Base::Result<ResolvedMember> ResolvePropertyOrEvent(
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
    ::Aero::Meta::Registry& domain) noexcept {
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

// Consolidated private Markup runtime and schema contract.

// ===== Loader contract =====
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Integration/SourceProvider.hpp>

#include <Aero/Markup.hpp>
#include <Aero/Markup.hpp>

#include <atomic>
#include <cstdint>

namespace Aero { class ResourceDictionary; }

namespace Aero::Core {
class DependencyPropertyRegistry;
class Dispatcher;
class EffectiveValueEngine;
}

namespace Aero::Markup {

class Schema;

class AERO_API EffectLifetime final : public Base::Object {
public:
    EffectLifetime() noexcept = default;
    ~EffectLifetime() noexcept override = default;

    bool IsActive() const noexcept {
        return active_.load(std::memory_order_acquire);
    }
    void Invalidate() noexcept {
        active_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> active_{true};
};

enum class EffectCommitMode : std::uint8_t {
    Immediate = 0U,
    Deferred
};

using Source = Integration::Source;
using ISourceProvider = Integration::ISourceProvider;
using SourceProviderAdapter = Integration::SourceProviderAdapter;

struct SourceProviderResolution final {
    ISourceProvider* provider = nullptr;
    std::uint64_t cacheIdentity = 0U;
};

struct SourceProviderRegistration final {
    Base::String scheme;
    Base::String assembly;
    ISourceProvider* provider = nullptr;
};

class AERO_API SourceProviders final {
public:
    Base::Result<void> TryRegister(
        ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;
    Base::Result<void> TryRegister(
        SourceProviderAdapter& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept {
        if (!provider.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Markup source provider is invalid");
        }
        return TryRegister(
            static_cast<ISourceProvider&>(provider),
            scheme,
            assembly);
    }
    Base::Result<SourceProviderResolution> ResolveDetailed(
        const Base::ResourceUri& uri) const noexcept;
    Base::Result<ISourceProvider*> Resolve(
        const Base::ResourceUri& uri) const noexcept;

    std::uint32_t ProviderCount() const noexcept {
        return registrations_.Size();
    }

private:
    Base::Vector<SourceProviderRegistration> registrations_;
};

class AERO_API EmbeddedSourceProvider final
    : public ISourceProvider {
public:
    Base::Result<void> TryAdd(
        const Base::ResourceUri& uri,
        Base::Span<const std::uint8_t> bytes,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> TryAddText(
        const Base::ResourceUri& uri,
        Base::StringView text,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override {
        return cacheIdentity_;
    }

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t SourceCount() const noexcept {
        return entries_.Size();
    }

private:
    struct Entry final {
        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> bytes;
        std::uint64_t revision = 0U;
    };

    Base::Vector<Entry> entries_;
    std::uint64_t cacheIdentity_ = UINT64_C(0xA3E0E4BEDDED0001);
    bool frozen_ = false;
};

class AERO_API FileSourceProvider final
    : public ISourceProvider {
public:
    explicit FileSourceProvider(
        std::uint64_t maxFileBytes =
            64ULL * 1024ULL * 1024ULL) noexcept
        : maxFileBytes_(maxFileBytes) {}

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override {
        return Base::MixHash64(
            UINT64_C(0xA3E0F11E00000001) ^ maxFileBytes_);
    }

private:
    std::uint64_t maxFileBytes_ = 0U;
};

struct DocumentCacheLimits final {
    std::uint32_t maxEntries = 256U;
    std::uint64_t maxCompiledBytes =
        64ULL * 1024ULL * 1024ULL;
};

struct DocumentCacheStatistics final {
    std::uint32_t entryCount = 0U;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t hitCount = 0U;
    std::uint64_t missCount = 0U;
    std::uint64_t storeCount = 0U;
    std::uint64_t invalidationCount = 0U;
    std::uint64_t evictionCount = 0U;
    std::uint64_t generation = 0U;
};

struct DocumentCacheLookup final {
    bool hit = false;
    std::uint64_t sourceRevision = 0U;
    CompiledDocument document;
};

class AERO_API DependencyGraph final {
public:
    explicit DependencyGraph(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~DependencyGraph() noexcept;

    DependencyGraph(DependencyGraph&& other) noexcept;
    DependencyGraph& operator=(
        DependencyGraph&& other) noexcept;

    DependencyGraph(const DependencyGraph&) = delete;
    DependencyGraph& operator=(const DependencyGraph&) = delete;

    Base::Result<void> Update(
        const Base::ResourceUri& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;
    bool Remove(
        const Base::ResourceUri& document) noexcept;
    void Clear() noexcept;

    Base::Result<void> CopyDependencies(
        const Base::ResourceUri& document,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CopyDependents(
        const Base::ResourceUri& dependency,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    std::uint32_t NodeCount() const noexcept;
    std::uint64_t Generation() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

class AERO_API DocumentCache final {
public:
    explicit DocumentCache(
        Base::IAllocator* allocator = nullptr,
        const DocumentCacheLimits& limits = {}) noexcept;
    ~DocumentCache() noexcept;

    DocumentCache(DocumentCache&& other) noexcept;
    DocumentCache& operator=(
        DocumentCache&& other) noexcept;

    DocumentCache(const DocumentCache&) = delete;
    DocumentCache& operator=(const DocumentCache&) = delete;

    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept {
        return Lookup(
            uri, sourceRevision, 0U, domain, limits);
    }
    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const ::Aero::Meta::Registry& domain,
        const CompiledDocumentLimits& limits = {}) noexcept;
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept {
        return Store(
            uri, sourceRevision, 0U,
            document, dependencies);
    }
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents = true) noexcept;
    void Clear() noexcept;

    bool Contains(
        const Base::ResourceUri& uri) const noexcept;
    bool TryGetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t& revision) const noexcept {
        return TryGetSourceRevision(uri, 0U, revision);
    }
    bool TryGetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    const DependencyGraph& Dependencies() const noexcept;
    DocumentCacheStatistics Statistics() const noexcept;
    const DocumentCacheLimits& Limits() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

class AERO_API Loader final {
public:
    Loader(
        Schema& schema,
        SourceProviders& providers,
        Core::IDiagnosticSink* diagnostics = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Loader() noexcept;

    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;
    Loader(Loader&&) = delete;
    Loader& operator=(Loader&&) = delete;

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> Load(
        const Base::ResourceUri& uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const LoadOptions& options = {}) noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup


// ===== LoaderResult contract =====
// Private transaction result consumed by Loader, UiDocument, and View.

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include "gui/PropertyInternal.hpp"

#include <Aero/Markup.hpp>
#include "gui/ElementInternal.hpp"

#include <utility>

namespace Aero::Detail {

struct VisualEdge final {
    UIElement* parent = nullptr;
    UIElement* child = nullptr;
    ElementAttachment state;
};

} // namespace Aero::Detail

namespace Aero::Markup {

using EffectCommitCallback = Base::Result<std::uint64_t> (*)(
    void* context) noexcept;
using EffectRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using EffectCleanupCallback = void (*)(void* context) noexcept;

struct VisualContentEdge final {
    Base::Ref<Base::Object> parentOwner;
    Base::Ref<Base::Object> childOwner;
    ::Aero::Meta::Registry* metadata = nullptr;
    Core::MemberId member = Core::InvalidMemberId;
    bool property = false;
};

// Markup-owned declaration result for visual content. The plan intentionally
// stores only content ownership and UI mount edges; the UI runtime owns
// the actual attach/detach sequence through the owning ElementTree.
struct VisualContentPlan final {
    Base::Vector<VisualContentEdge> contentEdges;
    Base::Vector<Aero::Detail::VisualEdge> mountEdges;
    Base::Vector<Aero::Visual*> nodes;

    Base::Result<void> TryReserve(
        std::uint32_t contentEdgeCount,
        std::uint32_t mountEdgeCount,
        std::uint32_t nodeCount) noexcept;
    Base::Result<void> TryAddNode(
        Aero::Visual& node) noexcept;
    void ReleaseContent() noexcept;
    void Clear() noexcept;
    std::uint32_t EdgeCount() const noexcept {
        return mountEdges.Size();
    }
    std::uint32_t NodeCount() const noexcept {
        return nodes.Size();
    }
};


struct CommittedEffect final {
    Base::Ref<EffectLifetime> lifetime;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    ::Aero::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    Core::PropertyExpression pendingExpression;
    void* context = nullptr;
    std::uint64_t token = 0U;
    EffectCommitCallback commit = nullptr;
    EffectRollbackCallback rollback = nullptr;
    EffectCleanupCallback cleanup = nullptr;
    bool committed = false;

    Base::Result<void> Commit() noexcept {
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

    void Rollback() noexcept {
        const bool runtimeActive = !lifetime || lifetime->IsActive();
        if (committed && runtimeActive) {
            if (effectiveValues != nullptr && target != nullptr) {
                static_cast<void>(effectiveValues->ClearLocalExpression(
                    *target, property));
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
        pendingExpression = {};
        context = nullptr;
        token = 0U;
        commit = nullptr;
        rollback = nullptr;
        cleanup = nullptr;
        committed = false;
    }
};

class CommittedEffectPlan final {
public:
    CommittedEffectPlan() noexcept = default;
    ~CommittedEffectPlan() noexcept { Rollback(); }

    CommittedEffectPlan(
        CommittedEffectPlan&& other) noexcept
        : effects_(std::move(other.effects_)) {}
    CommittedEffectPlan& operator=(
        CommittedEffectPlan&& other) noexcept {
        if (this == &other) return *this;
        Rollback();
        effects_ = std::move(other.effects_);
        return *this;
    }

    CommittedEffectPlan(const CommittedEffectPlan&) = delete;
    CommittedEffectPlan& operator=(
        const CommittedEffectPlan&) = delete;

    Base::Vector<CommittedEffect>& Items() noexcept { return effects_; }
    const Base::Vector<CommittedEffect>& Items() const noexcept {
        return effects_;
    }
    Base::Result<void> Commit() noexcept {
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
    void Rollback() noexcept {
        for (std::uint32_t index = effects_.Size(); index > 0U; --index) {
            effects_[index - 1U].Rollback();
        }
        effects_.Clear();
    }
    std::uint32_t Size() const noexcept { return effects_.Size(); }

private:
    Base::Vector<CommittedEffect> effects_;
};

// Ownership returned by a successful XAML load. The object writer remains a
// short-lived loading session; mounted runtimes keep names, resources, and the
// visual content plan here instead of reaching back into Markup services.
struct LoaderResult final {
    Base::Ref<Base::Object> root;
    ::Aero::Meta::Registry* metadata = nullptr;
    Aero::NameScope names;
    Aero::ResourceDictionary resources;
    VisualContentPlan visualContent;
    CommittedEffectPlan effects;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;
    Base::Ref<EffectLifetime> runtimeLifetime;
    bool hasDeferredStaticResources = false;

    void Clear() noexcept {
        effects.Rollback();
        root.Reset();
        metadata = nullptr;
        names.Clear();
        resources.Clear();
        visualContent.ReleaseContent();
        visualContent.Clear();
        canonicalUri = {};
        dependencies.Clear();
        runtimeLifetime.Reset();
        hasDeferredStaticResources = false;
    }
};

} // namespace Aero::Markup


// ===== LoadState contract =====
#include "gui/ElementInternal.hpp"





namespace Aero::Markup {

using LoadFinalizeCallback = Base::Result<void> (*)(
    LoaderResult& result,
    void* context) noexcept;

struct LoadState final {
    const Aero::ResourceDictionary* resources = nullptr;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::Detail::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Base::Object* templatedParent = nullptr;
    Base::Ref<Base::Object> existingRoot;
    Base::Ref<EffectLifetime> effectLifetime;
    EffectCommitMode effectCommitMode =
        EffectCommitMode::Immediate;
    DocumentCache* documentCache = nullptr;
    Core::Dispatcher* dispatcher = nullptr;
    Core::DependencyPropertyRegistry* dependencyProperties = nullptr;
    std::uint32_t maxObjects = UINT32_MAX;
    bool deferUnresolvedStaticResources = false;
    LoadFinalizeCallback finalize = nullptr;
    void* finalizeContext = nullptr;
};

} // namespace Aero::Markup


// ===== LoadInternals contract =====
#include <Aero/Markup.hpp>



namespace Aero::Markup::Detail {

class LoadOptionsPrivate final {
public:
    static void SetContext(
        LoadOptions& options,
        const LoadState* context) noexcept {
        options.context_ = context;
    }

    static const LoadState& Context(
        const LoadOptions& options) noexcept {
        static const LoadState empty;
        return options.context_ != nullptr
            ? *static_cast<const LoadState*>(options.context_)
            : empty;
    }
};

} // namespace Aero::Markup::Detail


// ===== GuiSchema contract =====
#include "runtime/modules/ModuleSet.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Meta { class Registry; class Registration; }


namespace Aero::Markup {
class Schema;
}

namespace Aero {

struct GuiSchemaOptions final {
    Base::IAllocator* allocator = nullptr;
};

class GuiSchema final {
public:
    explicit GuiSchema(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GuiSchema() noexcept;

    GuiSchema(const GuiSchema&) = delete;
    GuiSchema& operator=(const GuiSchema&) = delete;

    Base::Result<void> Prepare(
        const ModuleSet& modules) noexcept;
    Base::Result<void> Finalize(
        const GuiSchemaOptions& inputs) noexcept;
    bool IsPrepared() const noexcept;
    bool IsFrozen() const noexcept;
    ::Aero::Meta::Registry& Metadata() noexcept;
    const ::Aero::Meta::Registry& Metadata() const noexcept;
    Markup::Schema& Schema() noexcept;
    const Markup::Schema& Schema() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero


// ===== ResourceSupport contract =====
// Private XAML resource-scope registration.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Markup {

class Schema;

// Installs the ResourceDictionary schema adapters shared by runtime, compiled,
// application and theme XAML.
class AERO_API ResourceExtension final {
public:
    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    Schema* schema_ = nullptr;
};

} // namespace Aero::Markup


// ===== StaticResourceObject contract =====
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Markup::Detail {

// Supports the object-element form used by the reference theme, for example
// <StaticResource ResourceKey="Anim.Expand.Vertical.Loaded"/>.
class StaticResourceObject final : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        StaticResourceObject,
        ::Aero::DependencyObject,
        "urn:aero",
        "StaticResource")
public:
    StaticResourceObject() noexcept
        : DependencyObject(StaticTypeId()) {}

    Base::StringView ResourceKey() const noexcept {
        return GetValueOr(ResourceKeyProperty, Base::StringView{});
    }
    Base::Result<void> SetResourceKey(
        Base::StringView value) noexcept {
        return SetValue(ResourceKeyProperty, value);
    }

    inline static constexpr Members::Property<Base::String>
        ResourceKeyProperty{"ResourceKey"};
};

} // namespace Aero::Markup::Detail


// ===== UiObjectModel contract =====
// Private registration bridge for UI authoring objects.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include "gui/MetadataInternal.hpp"
#include <Aero/DependencyProperty.hpp>

namespace Aero::Markup {

class Schema;

struct UiObjectModelOptions final {
    UiObjectModelOptions() noexcept = default;
    UiObjectModelOptions(
        ::Aero::Meta::Registry* metadata,
        Core::DependencyPropertyRegistry* dependencyProperties,
        Base::IAllocator* programAllocator = nullptr) noexcept
        : metadata(metadata),
          properties(dependencyProperties),
          allocator(programAllocator) {}

    ::Aero::Meta::Registry* metadata = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    Base::IAllocator* allocator = nullptr;
};

// Optional registration override used by schema hosts that expose a custom
// Style/Setter model. Product runtimes use Register(schema), which
// registers Aero's complete Style/Trigger/Template object model.
struct UiObjectModelTypes final {
    Core::TypeId style = Core::InvalidTypeId;
    Core::TypeId setter = Core::InvalidTypeId;
    Core::TypeId trigger = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty;
    bool includeTemplates = true;
};

// Owns the schema adapters for the UI XAML object model. Parsing,
// compiled XAML, themes, and application resources all register this same
// object model instead of constructing independent Style or Template paths.
class AERO_API UiObjectModel final {
public:
    explicit UiObjectModel(
        const UiObjectModelOptions& options) noexcept;
    ~UiObjectModel() noexcept;

    UiObjectModel(
        const UiObjectModel&) = delete;
    UiObjectModel& operator=(
        const UiObjectModel&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;
    Base::Result<void> Register(
        Schema& schema,
        const UiObjectModelTypes& types) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool optionsValid_ = false;
};

} // namespace Aero::Markup


// ===== UiObjectModelInternal contract =====


#include <Aero/Styling.hpp>

namespace Aero::Markup::Detail {

class XamlStyleSchemaFacet final {
public:
    explicit XamlStyleSchemaFacet(
        const UiObjectModelOptions& options) noexcept;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId styleType,
        Core::TypeId setterType,
        Core::DependencyPropertyHandle styleProperty,
        Core::TypeId triggerType) noexcept;
private:
    UiObjectModelOptions options_;
    Schema* schema_ = nullptr;
    Core::TypeId styleType_ = Core::InvalidTypeId;
    Core::TypeId setterType_ = Core::InvalidTypeId;
    Core::TypeId triggerType_ = Core::InvalidTypeId;

    Base::Result<void> FinalizeStyle(
        Aero::Style& style) noexcept;
    Base::Result<Core::PropertyValue> ConvertValueForProperty(
        const Core::Value& value,
        Core::TypeId targetType,
        Base::StringView propertyName) const noexcept;

    static Base::Result<void> EndStyleInit(
        Base::Object& object,
        void* context) noexcept;
};

class XamlTemplateSchemaFacet final {
public:
    XamlTemplateSchemaFacet(
        ::Aero::Meta::Registry& metadata,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlTemplateSchemaFacet() noexcept;

    XamlTemplateSchemaFacet(
        const XamlTemplateSchemaFacet&) = delete;
    XamlTemplateSchemaFacet& operator=(
        const XamlTemplateSchemaFacet&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup::Detail


// ===== XamlDocumentInternal contract =====
#include <Aero/Markup.hpp>



namespace Aero::Detail {

class XamlDocumentPrivate final {
public:
    static Base::Result<UiDocument> Adopt(
        Markup::LoaderResult&& result,
        Base::IAllocator& allocator) noexcept;
    static Markup::LoaderResult Take(
        UiDocument& document) noexcept;
    static const Markup::EffectLifetime* RuntimeLifetime(
        const UiDocument& document) noexcept;
};

} // namespace Aero::Detail

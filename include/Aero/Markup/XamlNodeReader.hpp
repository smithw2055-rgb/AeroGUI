#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdint>

namespace Aero::Markup {

enum class XamlNodeKind : std::uint8_t {
    None = 0U,
    NamespaceDeclaration,
    StartObject,
    EndObject,
    StartMember,
    EndMember,
    Value,
    EndOfDocument
};

namespace XamlNodeDiagnosticCodes {
inline constexpr Core::DiagnosticCode UnboundNamespacePrefix =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 101U);
inline constexpr Core::DiagnosticCode InvalidNamespaceDeclaration =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 102U);
inline constexpr Core::DiagnosticCode DuplicateNamespacePrefix =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 103U);
inline constexpr Core::DiagnosticCode InvalidNodeStreamState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 104U);
} // namespace XamlNodeDiagnosticCodes

class AERO_API XamlQualifiedName final {
public:
    explicit XamlQualifiedName(Base::IAllocator* allocator = nullptr) noexcept
        : prefix_(allocator), localName_(allocator), namespaceUri_(allocator) {}

    XamlQualifiedName(XamlQualifiedName&&) noexcept = default;
    XamlQualifiedName& operator=(XamlQualifiedName&&) noexcept = default;

    XamlQualifiedName(const XamlQualifiedName&) = delete;
    XamlQualifiedName& operator=(const XamlQualifiedName&) = delete;

    AERO_NODISCARD Base::StringView Prefix() const noexcept {
        return prefix_.View();
    }
    AERO_NODISCARD Base::StringView LocalName() const noexcept {
        return localName_.View();
    }
    AERO_NODISCARD Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    AERO_NODISCARD bool HasNamespace() const noexcept {
        return !namespaceUri_.Empty();
    }

private:
    friend class XamlNode;
    friend class XamlNodeReader;

    void Clear() noexcept;

    Base::String prefix_;
    Base::String localName_;
    Base::String namespaceUri_;
};

class AERO_API XamlNode final {
public:
    explicit XamlNode(Base::IAllocator* allocator = nullptr) noexcept
        : name_(allocator), namespacePrefix_(allocator),
          namespaceUri_(allocator), value_(allocator) {}

    XamlNode(XamlNode&&) noexcept = default;
    XamlNode& operator=(XamlNode&&) noexcept = default;

    XamlNode(const XamlNode&) = delete;
    XamlNode& operator=(const XamlNode&) = delete;

    void Clear() noexcept;

    AERO_NODISCARD XamlNodeKind Kind() const noexcept { return kind_; }
    AERO_NODISCARD const XamlQualifiedName& Name() const noexcept {
        return name_;
    }
    AERO_NODISCARD Base::StringView NamespacePrefix() const noexcept {
        return namespacePrefix_.View();
    }
    AERO_NODISCARD Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    AERO_NODISCARD Base::StringView Value() const noexcept {
        return value_.View();
    }
    AERO_NODISCARD Core::SourceSpan Source() const noexcept {
        return source_;
    }
    AERO_NODISCARD bool IsFromAttribute() const noexcept {
        return fromAttribute_;
    }

private:
    friend class XamlNodeReader;

    XamlNodeKind kind_ = XamlNodeKind::None;
    XamlQualifiedName name_;
    Base::String namespacePrefix_;
    Base::String namespaceUri_;
    Base::String value_;
    Core::SourceSpan source_;
    bool fromAttribute_ = false;
};

class AERO_API XamlNodeReader final {
public:
    explicit XamlNodeReader(
        IXmlTokenizer& tokenizer,
        Core::IDiagnosticSink* diagnostics = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;

    void Reset() noexcept;

    AERO_NODISCARD Base::Result<XamlNodeKind> Read(
        XamlNode& node) noexcept;
    AERO_NODISCARD std::uint32_t ObjectDepth() const noexcept {
        return scopes_.Size();
    }

private:
    struct NamespaceBinding final {
        explicit NamespaceBinding(Base::IAllocator* allocator) noexcept
            : prefix(allocator), uri(allocator) {}

        NamespaceBinding(NamespaceBinding&&) noexcept = default;
        NamespaceBinding& operator=(NamespaceBinding&&) noexcept = default;

        NamespaceBinding(const NamespaceBinding&) = delete;
        NamespaceBinding& operator=(const NamespaceBinding&) = delete;

        Base::String prefix;
        Base::String uri;
    };

    struct ScopeFrame final {
        explicit ScopeFrame(Base::IAllocator* allocator) noexcept
            : objectName(allocator) {}

        ScopeFrame(ScopeFrame&&) noexcept = default;
        ScopeFrame& operator=(ScopeFrame&&) noexcept = default;

        ScopeFrame(const ScopeFrame&) = delete;
        ScopeFrame& operator=(const ScopeFrame&) = delete;

        std::uint32_t bindingStart = 0U;
        XamlQualifiedName objectName;
    };

    IXmlTokenizer* tokenizer_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    XmlToken xmlToken_;
    Base::Vector<XamlNode> pending_;
    Base::Vector<NamespaceBinding> bindings_;
    Base::Vector<ScopeFrame> scopes_;
    std::uint32_t pendingIndex_ = 0U;
    bool ended_ = false;
    bool failed_ = false;

    AERO_NODISCARD Base::Result<void> QueueStartElement(
        const XmlToken& token) noexcept;
    AERO_NODISCARD Base::Result<void> QueueEndElement(
        const XmlToken& token) noexcept;
    AERO_NODISCARD Base::Result<void> QueueText(
        const XmlToken& token) noexcept;
    AERO_NODISCARD Base::Result<void> QueueEndOfDocument(
        const XmlToken& token) noexcept;
    AERO_NODISCARD Base::Result<void> QueueNamespaceDeclaration(
        Base::StringView prefix,
        Base::StringView uri,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> QueueObjectNode(
        XamlNodeKind kind,
        const XamlQualifiedName& name,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> QueueMemberNodes(
        const XmlAttribute& attribute) noexcept;
    AERO_NODISCARD Base::Result<void> AppendPending(
        XamlNode&& node) noexcept;

    AERO_NODISCARD Base::Result<void> AddNamespaceBinding(
        Base::StringView prefix,
        Base::StringView uri,
        std::uint32_t bindingStart,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> ResolveQualifiedName(
        Base::StringView qualifiedName,
        bool useDefaultNamespace,
        Core::SourceSpan source,
        XamlQualifiedName& output) noexcept;
    AERO_NODISCARD Base::Result<void> CopyQualifiedName(
        const XamlQualifiedName& source,
        XamlQualifiedName& output) noexcept;
    AERO_NODISCARD Base::StringView LookupNamespace(
        Base::StringView prefix,
        bool& found) const noexcept;
    AERO_NODISCARD bool IsNamespaceDeclaration(
        Base::StringView attributeName,
        Base::StringView& prefix) const noexcept;
    void PopBindings(std::uint32_t bindingStart) noexcept;

    AERO_NODISCARD Base::Result<XamlNodeKind> EmitPending(
        XamlNode& node) noexcept;
    AERO_NODISCARD Base::Status Failure(
        Base::ErrorCode error,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

} // namespace Aero::Markup

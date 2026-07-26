#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Markup/Parsing/XamlNode.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>

#include <cstdint>

namespace Aero::Markup {

class AERO_API XamlNodeReader final {
public:
    explicit XamlNodeReader(
        IXmlTokenizer& tokenizer,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    void Reset() noexcept;

    Base::Result<XamlNodeKind> Read(
        XamlNode& node) noexcept;
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
        XamlQualifiedName objectName;
    };

    IXmlTokenizer* tokenizer_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    XmlToken xmlToken_;
    Base::Vector<XamlNode> pending_;
    Base::Vector<NamespaceBinding> bindings_;
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
        XamlNodeKind kind,
        const XamlQualifiedName& name,
        Core::SourceSpan source) noexcept;
    Base::Result<void> QueueMemberNodes(
        const XmlAttribute& attribute) noexcept;
    Base::Result<void> AppendPending(
        XamlNode&& node) noexcept;

    Base::Result<void> AddNamespaceBinding(
        Base::StringView prefix,
        Base::StringView uri,
        std::uint32_t bindingStart,
        Core::SourceSpan source) noexcept;
    Base::Result<void> ResolveQualifiedName(
        Base::StringView qualifiedName,
        bool useDefaultNamespace,
        Core::SourceSpan source,
        XamlQualifiedName& output) noexcept;
    Base::Result<void> CopyQualifiedName(
        const XamlQualifiedName& source,
        XamlQualifiedName& output) noexcept;
    Base::StringView LookupNamespace(
        Base::StringView prefix,
        bool& found) const noexcept;
    bool IsNamespaceDeclaration(
        Base::StringView attributeName,
        Base::StringView& prefix) const noexcept;
    void PopBindings(std::uint32_t bindingStart) noexcept;

    Base::Result<XamlNodeKind> EmitPending(
        XamlNode& node) noexcept;
    Base::Status Failure(
        Base::ErrorCode error,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

} // namespace Aero::Markup

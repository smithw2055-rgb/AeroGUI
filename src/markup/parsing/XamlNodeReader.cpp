#include <Aero/Markup/Parsing/XamlNodeReader.hpp>

#include <cstring>
#include <utility>

namespace Aero::Markup {
namespace {

constexpr Base::StringView XmlPrefix("xml");
constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
constexpr Base::StringView XmlnsPrefix("xmlns");
constexpr Base::StringView XmlnsNamespaceUri(
    "http://www.w3.org/2000/xmlns/");

constexpr Base::StringView MessageUnboundPrefix(
    "XAML qualified name uses an unbound XML namespace prefix");
constexpr Base::StringView MessageInvalidNamespace(
    "XML namespace declaration is invalid for the XAML node stream");
constexpr Base::StringView MessageDuplicateNamespace(
    "XML element declares the same namespace prefix more than once");
constexpr Base::StringView MessageInvalidState(
    "XAML node reader encountered an invalid tokenizer or scope state");
constexpr Base::StringView MessageReaderFailed(
    "XAML node reader cannot continue after a conversion failure");

bool StartsWith(
    Base::StringView value,
    const char* literal,
    std::uint32_t literalLength) noexcept {
    return value.SizeBytes() >= literalLength &&
        (literalLength == 0U ||
            std::memcmp(value.Data(), literal, literalLength) == 0);
}

} // namespace

void XamlQualifiedName::Clear() noexcept {
    prefix_.Clear();
    localName_.Clear();
    namespaceUri_.Clear();
}

void XamlNode::Clear() noexcept {
    kind_ = XamlNodeKind::None;
    name_.Clear();
    namespacePrefix_.Clear();
    namespaceUri_.Clear();
    value_.Clear();
    source_ = {};
    fromAttribute_ = false;
}

Base::Result<XamlNode> XamlNode::TryClone(
    const XamlNode& source) noexcept {
    XamlNode clone;
    clone.kind_ = source.kind_;
    clone.source_ = source.source_;
    clone.fromAttribute_ = source.fromAttribute_;
    Base::Result<void> copied =
        clone.name_.prefix_.TryAssign(
            source.name_.prefix_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.name_.localName_.TryAssign(
        source.name_.localName_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.name_.namespaceUri_.TryAssign(
        source.name_.namespaceUri_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.namespacePrefix_.TryAssign(
        source.namespacePrefix_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.namespaceUri_.TryAssign(
        source.namespaceUri_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.value_.TryAssign(source.value_.View());
    if (!copied) return copied.GetStatus();
    return clone;
}

XamlNodeReader::XamlNodeReader(
    IXmlTokenizer& tokenizer,
    Core::IDiagnosticSink* diagnostics) noexcept
    : tokenizer_(&tokenizer),
      diagnostics_(diagnostics),
      xmlToken_(),
      pending_(),
      bindings_(),
      scopes_() {}

void XamlNodeReader::Reset() noexcept {
    xmlToken_.Clear();
    pending_.Clear();
    bindings_.Clear();
    scopes_.Clear();
    pendingIndex_ = 0U;
    ended_ = false;
    failed_ = false;
}

Base::Result<XamlNodeKind> XamlNodeReader::Read(XamlNode& node) noexcept {
    if (failed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageReaderFailed.Data());
    }

    if (pendingIndex_ < pending_.Size()) {
        return EmitPending(node);
    }

    pending_.Clear();
    pendingIndex_ = 0U;

    if (ended_) {
        node.Clear();
        node.kind_ = XamlNodeKind::EndOfDocument;
        return node.kind_;
    }

    Base::Result<XmlTokenKind> tokenResult = tokenizer_->Read(xmlToken_);
    if (!tokenResult) {
        failed_ = true;
        return tokenResult.GetStatus();
    }

    Base::Result<void> queueResult;
    switch (tokenResult.Value()) {
    case XmlTokenKind::StartElement:
        queueResult = QueueStartElement(xmlToken_);
        break;
    case XmlTokenKind::EndElement:
        queueResult = QueueEndElement(xmlToken_);
        break;
    case XmlTokenKind::Text:
        queueResult = QueueText(xmlToken_);
        break;
    case XmlTokenKind::EndOfDocument:
        queueResult = QueueEndOfDocument(xmlToken_);
        break;
    case XmlTokenKind::None:
        queueResult = Failure(
            Base::ErrorCode::InvalidState,
            XamlNodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            xmlToken_.Source());
        break;
    }

    if (!queueResult) {
        failed_ = true;
        return queueResult.GetStatus();
    }
    if (pending_.Empty()) {
        failed_ = true;
        return Failure(
            Base::ErrorCode::InternalError,
            XamlNodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            xmlToken_.Source());
    }

    return EmitPending(node);
}

Base::Result<void> XamlNodeReader::QueueStartElement(
    const XmlToken& token) noexcept {
    const std::uint32_t bindingStart = bindings_.Size();

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (!IsNamespaceDeclaration(attribute.Name(), prefix)) {
            continue;
        }

        Base::Result<void> bindingResult = AddNamespaceBinding(
            prefix,
            attribute.Value(),
            bindingStart,
            attribute.Source());
        if (!bindingResult) {
            return bindingResult.GetStatus();
        }
    }

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (!IsNamespaceDeclaration(attribute.Name(), prefix)) {
            continue;
        }

        Base::Result<void> namespaceResult = QueueNamespaceDeclaration(
            prefix,
            attribute.Value(),
            attribute.Source());
        if (!namespaceResult) {
            return namespaceResult.GetStatus();
        }
    }

    ScopeFrame frame;
    frame.bindingStart = bindingStart;
    Base::Result<void> resolveObject = ResolveQualifiedName(
        token.Name(),
        true,
        token.NameSource(),
        frame.objectName);
    if (!resolveObject) {
        return resolveObject.GetStatus();
    }

    Base::Result<void> startResult = QueueObjectNode(
        XamlNodeKind::StartObject,
        frame.objectName,
        token.Source());
    if (!startResult) {
        return startResult.GetStatus();
    }

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (IsNamespaceDeclaration(attribute.Name(), prefix)) {
            continue;
        }

        Base::Result<void> memberResult = QueueMemberNodes(attribute);
        if (!memberResult) {
            return memberResult.GetStatus();
        }
    }

    if (token.IsEmptyElement()) {
        Base::Result<void> endResult = QueueObjectNode(
            XamlNodeKind::EndObject,
            frame.objectName,
            token.Source());
        PopBindings(bindingStart);
        return endResult;
    }

    Base::Result<void> scopeResult = scopes_.TryPushBack(std::move(frame));
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }
    return {};
}

Base::Result<void> XamlNodeReader::QueueEndElement(
    const XmlToken& token) noexcept {
    if (scopes_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            XamlNodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    ScopeFrame& frame = scopes_.Back();
    Base::Result<void> endResult = QueueObjectNode(
        XamlNodeKind::EndObject,
        frame.objectName,
        token.Source());
    if (!endResult) {
        return endResult.GetStatus();
    }

    const std::uint32_t bindingStart = frame.bindingStart;
    scopes_.PopBack();
    PopBindings(bindingStart);
    return {};
}

Base::Result<void> XamlNodeReader::QueueText(
    const XmlToken& token) noexcept {
    XamlNode node;
    node.kind_ = XamlNodeKind::Value;
    node.source_ = token.Source();
    node.fromAttribute_ = false;
    Base::Result<void> assignResult = node.value_.TryAssignUnchecked(token.Text());
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> XamlNodeReader::QueueEndOfDocument(
    const XmlToken& token) noexcept {
    if (!scopes_.Empty() || !bindings_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            XamlNodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    XamlNode node;
    node.kind_ = XamlNodeKind::EndOfDocument;
    node.source_ = token.Source();
    return AppendPending(std::move(node));
}

Base::Result<void> XamlNodeReader::QueueNamespaceDeclaration(
    Base::StringView prefix,
    Base::StringView uri,
    Core::SourceSpan source) noexcept {
    XamlNode node;
    node.kind_ = XamlNodeKind::NamespaceDeclaration;
    node.source_ = source;

    Base::Result<void> prefixResult = node.namespacePrefix_.TryAssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = node.namespaceUri_.TryAssignUnchecked(uri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> XamlNodeReader::QueueObjectNode(
    XamlNodeKind kind,
    const XamlQualifiedName& name,
    Core::SourceSpan source) noexcept {
    XamlNode node;
    node.kind_ = kind;
    node.source_ = source;
    Base::Result<void> copyResult = CopyQualifiedName(name, node.name_);
    if (!copyResult) {
        return copyResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> XamlNodeReader::QueueMemberNodes(
    const XmlAttribute& attribute) noexcept {
    XamlQualifiedName memberName;
    Base::Result<void> resolveResult = ResolveQualifiedName(
        attribute.Name(),
        false,
        attribute.NameSource(),
        memberName);
    if (!resolveResult) {
        return resolveResult.GetStatus();
    }

    XamlNode start;
    start.kind_ = XamlNodeKind::StartMember;
    start.source_ = attribute.NameSource();
    start.fromAttribute_ = true;
    Base::Result<void> startNameResult = CopyQualifiedName(memberName, start.name_);
    if (!startNameResult) {
        return startNameResult.GetStatus();
    }
    Base::Result<void> appendStart = AppendPending(std::move(start));
    if (!appendStart) {
        return appendStart.GetStatus();
    }

    XamlNode value;
    value.kind_ = XamlNodeKind::Value;
    value.source_ = attribute.ValueSource();
    value.fromAttribute_ = true;
    Base::Result<void> valueResult = value.value_.TryAssignUnchecked(attribute.Value());
    if (!valueResult) {
        return valueResult.GetStatus();
    }
    Base::Result<void> appendValue = AppendPending(std::move(value));
    if (!appendValue) {
        return appendValue.GetStatus();
    }

    XamlNode end;
    end.kind_ = XamlNodeKind::EndMember;
    end.source_ = attribute.NameSource();
    end.fromAttribute_ = true;
    Base::Result<void> endNameResult = CopyQualifiedName(memberName, end.name_);
    if (!endNameResult) {
        return endNameResult.GetStatus();
    }
    return AppendPending(std::move(end));
}

Base::Result<void> XamlNodeReader::AppendPending(XamlNode&& node) noexcept {
    return pending_.TryPushBack(std::move(node));
}

Base::Result<void> XamlNodeReader::AddNamespaceBinding(
    Base::StringView prefix,
    Base::StringView uri,
    std::uint32_t bindingStart,
    Core::SourceSpan source) noexcept {
    if (prefix == XmlnsPrefix || uri == XmlnsNamespaceUri ||
        (!prefix.Empty() && uri.Empty()) ||
        (prefix == XmlPrefix && uri != XmlNamespaceUri) ||
        (prefix != XmlPrefix && uri == XmlNamespaceUri)) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XamlNodeDiagnosticCodes::InvalidNamespaceDeclaration,
            MessageInvalidNamespace,
            source);
    }

    for (std::uint32_t index = bindingStart; index < bindings_.Size(); ++index) {
        if (bindings_[index].prefix.View() == prefix) {
            return Failure(
                Base::ErrorCode::AlreadyExists,
                XamlNodeDiagnosticCodes::DuplicateNamespacePrefix,
                MessageDuplicateNamespace,
                source);
        }
    }

    NamespaceBinding binding;
    Base::Result<void> prefixResult = binding.prefix.TryAssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = binding.uri.TryAssignUnchecked(uri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return bindings_.TryPushBack(std::move(binding));
}

Base::Result<void> XamlNodeReader::ResolveQualifiedName(
    Base::StringView qualifiedName,
    bool useDefaultNamespace,
    Core::SourceSpan source,
    XamlQualifiedName& output) noexcept {
    std::uint32_t colon = qualifiedName.SizeBytes();
    for (std::uint32_t index = 0U; index < qualifiedName.SizeBytes(); ++index) {
        if (qualifiedName[index] == ':') {
            colon = index;
            break;
        }
    }

    const bool hasPrefix = colon < qualifiedName.SizeBytes();
    const Base::StringView prefix = hasPrefix
        ? qualifiedName.Substr(0U, colon)
        : Base::StringView();
    const Base::StringView localName = hasPrefix
        ? qualifiedName.Substr(
            colon + 1U,
            qualifiedName.SizeBytes() - colon - 1U)
        : qualifiedName;

    bool found = false;
    Base::StringView namespaceUri;
    if (hasPrefix) {
        namespaceUri = LookupNamespace(prefix, found);
        if (!found) {
            return Failure(
                Base::ErrorCode::NotFound,
                XamlNodeDiagnosticCodes::UnboundNamespacePrefix,
                MessageUnboundPrefix,
                source);
        }
    } else if (useDefaultNamespace) {
        namespaceUri = LookupNamespace({}, found);
        if (!found) {
            namespaceUri = {};
        }
    }

    output.Clear();
    Base::Result<void> prefixResult = output.prefix_.TryAssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> localResult = output.localName_.TryAssignUnchecked(localName);
    if (!localResult) {
        return localResult.GetStatus();
    }
    Base::Result<void> uriResult = output.namespaceUri_.TryAssignUnchecked(namespaceUri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return {};
}

Base::Result<void> XamlNodeReader::CopyQualifiedName(
    const XamlQualifiedName& source,
    XamlQualifiedName& output) noexcept {
    output.Clear();
    Base::Result<void> prefixResult = output.prefix_.TryAssignUnchecked(source.Prefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> localResult = output.localName_.TryAssignUnchecked(source.LocalName());
    if (!localResult) {
        return localResult.GetStatus();
    }
    Base::Result<void> uriResult = output.namespaceUri_.TryAssignUnchecked(source.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return {};
}

Base::StringView XamlNodeReader::LookupNamespace(
    Base::StringView prefix,
    bool& found) const noexcept {
    if (prefix == XmlPrefix) {
        found = true;
        return XmlNamespaceUri;
    }

    for (std::uint32_t index = bindings_.Size(); index > 0U; --index) {
        const NamespaceBinding& binding = bindings_[index - 1U];
        if (binding.prefix.View() == prefix) {
            found = true;
            return binding.uri.View();
        }
    }

    found = false;
    return {};
}

bool XamlNodeReader::IsNamespaceDeclaration(
    Base::StringView attributeName,
    Base::StringView& prefix) const noexcept {
    if (attributeName == XmlnsPrefix) {
        prefix = {};
        return true;
    }
    if (StartsWith(attributeName, "xmlns:", 6U) &&
        attributeName.SizeBytes() > 6U) {
        prefix = attributeName.Substr(6U, attributeName.SizeBytes() - 6U);
        return true;
    }
    return false;
}

void XamlNodeReader::PopBindings(std::uint32_t bindingStart) noexcept {
    while (bindings_.Size() > bindingStart) {
        bindings_.PopBack();
    }
}

Base::Result<XamlNodeKind> XamlNodeReader::EmitPending(
    XamlNode& node) noexcept {
    if (pendingIndex_ >= pending_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageInvalidState.Data());
    }

    node = std::move(pending_[pendingIndex_]);
    ++pendingIndex_;
    if (node.Kind() == XamlNodeKind::EndOfDocument) {
        ended_ = true;
    }
    return node.Kind();
}

Base::Status XamlNodeReader::Failure(
    Base::ErrorCode error,
    Core::DiagnosticCode diagnostic,
    Base::StringView message,
    Core::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<Core::Diagnostic> item = Core::Diagnostic::TryCreate(
            diagnostic,
            Core::DiagnosticSeverity::Error,
            message,
            source,
            Core::InvalidDiagnosticObjectId,
            Core::InvalidMemberId);
        if (!item) {
            return item.GetStatus();
        }

        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }
    return Base::Status::Failure(error, message.Data());
}

} // namespace Aero::Markup

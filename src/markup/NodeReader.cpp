#include <Aero/Markup/CompiledDocument.hpp>

// Canonical markup node reader implementation.

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
constexpr Base::StringView MarkupCompatibilityNamespaceUri(
    "http://schemas.openxmlformats.org/markup-compatibility/2006");

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

void QualifiedName::Clear() noexcept {
    prefix_.Clear();
    localName_.Clear();
    namespaceUri_.Clear();
}

void Node::Clear() noexcept {
    kind_ = NodeKind::None;
    name_.Clear();
    namespacePrefix_.Clear();
    namespaceUri_.Clear();
    value_.Clear();
    source_ = {};
    fromAttribute_ = false;
}

Base::Result<Node> Node::TryClone(
    const Node& source) noexcept {
    Node clone;
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

NodeReader::NodeReader(
    IXmlTokenizer& tokenizer,
    Core::IDiagnosticSink* diagnostics) noexcept
    : tokenizer_(&tokenizer),
      diagnostics_(diagnostics),
      xmlToken_(),
      pending_(),
      bindings_(),
      scopes_() {}

void NodeReader::Reset() noexcept {
    xmlToken_.Clear();
    pending_.Clear();
    bindings_.Clear();
    ignorableNamespaces_.Clear();
    scopes_.Clear();
    pendingIndex_ = 0U;
    ended_ = false;
    failed_ = false;
}

Base::Result<NodeKind> NodeReader::Read(Node& node) noexcept {
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
        node.kind_ = NodeKind::EndOfDocument;
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
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            xmlToken_.Source());
        break;
    }

    if (!queueResult) {
        failed_ = true;
        return queueResult.GetStatus();
    }
    if (pending_.Empty()) {
        // Ignorable markup-compatibility nodes and their whitespace produce
        // no XAML nodes. Continue until an observable node is available.
        return Read(node);
    }

    return EmitPending(node);
}

Base::Result<void> NodeReader::QueueStartElement(
    const XmlToken& token) noexcept {
    const std::uint32_t bindingStart = bindings_.Size();
    const std::uint32_t ignorableNamespaceStart =
        ignorableNamespaces_.Size();

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

    for (const XmlAttribute& attribute : token.Attributes()) {
        if (!IsMarkupCompatibilityIgnorable(attribute)) continue;
        Base::Result<void> ignored = AddIgnorableNamespaces(
            attribute.Value(), attribute.Source());
        if (!ignored) return ignored.GetStatus();
    }

    ScopeFrame frame;
    frame.bindingStart = bindingStart;
    frame.ignorableNamespaceStart = ignorableNamespaceStart;
    Base::Result<void> resolveObject = ResolveQualifiedName(
        token.Name(),
        true,
        token.NameSource(),
        frame.objectName);
    if (!resolveObject) {
        return resolveObject.GetStatus();
    }

    frame.ignored = IsIgnorableNamespace(
        frame.objectName.NamespaceUri()) ||
        (!scopes_.Empty() && scopes_.Back().ignored);

    if (!frame.ignored) {
        Base::Result<void> startResult = QueueObjectNode(
            NodeKind::StartObject,
            frame.objectName,
            token.Source());
        if (!startResult) {
            return startResult.GetStatus();
        }
    }

    if (!frame.ignored) {
        for (const XmlAttribute& attribute : token.Attributes()) {
            Base::StringView prefix;
            if (IsNamespaceDeclaration(attribute.Name(), prefix) ||
                IsMarkupCompatibilityIgnorable(attribute)) {
                continue;
            }

            QualifiedName attributeName;
            Base::Result<void> resolved = ResolveQualifiedName(
                attribute.Name(), false, attribute.NameSource(), attributeName);
            if (!resolved) return resolved.GetStatus();
            if (IsIgnorableNamespace(attributeName.NamespaceUri())) {
                continue;
            }

            Base::Result<void> memberResult = QueueMemberNodes(attribute);
            if (!memberResult) {
                return memberResult.GetStatus();
            }
        }
    }

    if (token.IsEmptyElement()) {
        Base::Result<void> endResult;
        if (!frame.ignored) {
            endResult = QueueObjectNode(
                NodeKind::EndObject,
                frame.objectName,
                token.Source());
        }
        PopBindings(bindingStart);
        PopIgnorableNamespaces(ignorableNamespaceStart);
        return endResult;
    }

    Base::Result<void> scopeResult = scopes_.TryPushBack(std::move(frame));
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }
    return {};
}

Base::Result<void> NodeReader::QueueEndElement(
    const XmlToken& token) noexcept {
    if (scopes_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    ScopeFrame& frame = scopes_.Back();
    Base::Result<void> endResult;
    if (!frame.ignored) {
        endResult = QueueObjectNode(
            NodeKind::EndObject,
            frame.objectName,
            token.Source());
    }
    if (!endResult) {
        return endResult.GetStatus();
    }

    const std::uint32_t bindingStart = frame.bindingStart;
    const std::uint32_t ignorableNamespaceStart =
        frame.ignorableNamespaceStart;
    scopes_.PopBack();
    PopBindings(bindingStart);
    PopIgnorableNamespaces(ignorableNamespaceStart);
    return {};
}

Base::Result<void> NodeReader::QueueText(
    const XmlToken& token) noexcept {
    if (!scopes_.Empty() && scopes_.Back().ignored) return {};
    Node node;
    node.kind_ = NodeKind::Value;
    node.source_ = token.Source();
    node.fromAttribute_ = false;
    Base::Result<void> assignResult = node.value_.TryAssignUnchecked(token.Text());
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueEndOfDocument(
    const XmlToken& token) noexcept {
    if (!scopes_.Empty() || !bindings_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    Node node;
    node.kind_ = NodeKind::EndOfDocument;
    node.source_ = token.Source();
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueNamespaceDeclaration(
    Base::StringView prefix,
    Base::StringView uri,
    Core::SourceSpan source) noexcept {
    Node node;
    node.kind_ = NodeKind::NamespaceDeclaration;
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

Base::Result<void> NodeReader::QueueObjectNode(
    NodeKind kind,
    const QualifiedName& name,
    Core::SourceSpan source) noexcept {
    Node node;
    node.kind_ = kind;
    node.source_ = source;
    Base::Result<void> copyResult = CopyQualifiedName(name, node.name_);
    if (!copyResult) {
        return copyResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueMemberNodes(
    const XmlAttribute& attribute) noexcept {
    QualifiedName memberName;
    Base::Result<void> resolveResult = ResolveQualifiedName(
        attribute.Name(),
        false,
        attribute.NameSource(),
        memberName);
    if (!resolveResult) {
        return resolveResult.GetStatus();
    }

    Node start;
    start.kind_ = NodeKind::StartMember;
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

    Node value;
    value.kind_ = NodeKind::Value;
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

    Node end;
    end.kind_ = NodeKind::EndMember;
    end.source_ = attribute.NameSource();
    end.fromAttribute_ = true;
    Base::Result<void> endNameResult = CopyQualifiedName(memberName, end.name_);
    if (!endNameResult) {
        return endNameResult.GetStatus();
    }
    return AppendPending(std::move(end));
}

Base::Result<void> NodeReader::AppendPending(Node&& node) noexcept {
    return pending_.TryPushBack(std::move(node));
}

Base::Result<void> NodeReader::AddNamespaceBinding(
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
            NodeDiagnosticCodes::InvalidNamespaceDeclaration,
            MessageInvalidNamespace,
            source);
    }

    for (std::uint32_t index = bindingStart; index < bindings_.Size(); ++index) {
        if (bindings_[index].prefix.View() == prefix) {
            return Failure(
                Base::ErrorCode::AlreadyExists,
                NodeDiagnosticCodes::DuplicateNamespacePrefix,
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

Base::Result<void> NodeReader::AddIgnorableNamespaces(
    Base::StringView prefixes,
    Core::SourceSpan source) noexcept {
    std::uint32_t begin = 0U;
    while (begin < prefixes.SizeBytes()) {
        while (begin < prefixes.SizeBytes() &&
               (prefixes[begin] == ' ' || prefixes[begin] == '\t' ||
                prefixes[begin] == '\r' || prefixes[begin] == '\n')) {
            ++begin;
        }
        const std::uint32_t tokenBegin = begin;
        while (begin < prefixes.SizeBytes() &&
               prefixes[begin] != ' ' && prefixes[begin] != '\t' &&
               prefixes[begin] != '\r' && prefixes[begin] != '\n') {
            ++begin;
        }
        if (tokenBegin == begin) continue;
        bool found = false;
        const Base::StringView uri = LookupNamespace(
            prefixes.Substr(tokenBegin, begin - tokenBegin), found);
        if (!found || uri.Empty()) {
            return Failure(
                Base::ErrorCode::NotFound,
                NodeDiagnosticCodes::UnboundNamespacePrefix,
                MessageUnboundPrefix, source);
        }
        Base::String copied;
        Base::Result<void> assigned = copied.TryAssign(uri);
        if (!assigned) return assigned.GetStatus();
        Base::Result<void> appended = ignorableNamespaces_.TryPushBack(
            std::move(copied));
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> NodeReader::ResolveQualifiedName(
    Base::StringView qualifiedName,
    bool useDefaultNamespace,
    Core::SourceSpan source,
    QualifiedName& output) noexcept {
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
                NodeDiagnosticCodes::UnboundNamespacePrefix,
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

Base::Result<void> NodeReader::CopyQualifiedName(
    const QualifiedName& source,
    QualifiedName& output) noexcept {
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

Base::StringView NodeReader::LookupNamespace(
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

bool NodeReader::IsNamespaceDeclaration(
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

bool NodeReader::IsIgnorableNamespace(
    Base::StringView uri) const noexcept {
    for (const Base::String& ignored : ignorableNamespaces_) {
        if (ignored.View() == uri) return true;
    }
    return false;
}

bool NodeReader::IsMarkupCompatibilityIgnorable(
    const XmlAttribute& attribute) const noexcept {
    QualifiedName name;
    Base::Result<void> resolved = const_cast<NodeReader*>(this)
        ->ResolveQualifiedName(
            attribute.Name(), false, attribute.NameSource(), name);
    return resolved &&
        name.NamespaceUri() == MarkupCompatibilityNamespaceUri &&
        name.LocalName() == Base::StringView("Ignorable");
}

void NodeReader::PopBindings(std::uint32_t bindingStart) noexcept {
    while (bindings_.Size() > bindingStart) {
        bindings_.PopBack();
    }
}

void NodeReader::PopIgnorableNamespaces(std::uint32_t start) noexcept {
    while (ignorableNamespaces_.Size() > start) {
        ignorableNamespaces_.PopBack();
    }
}

Base::Result<NodeKind> NodeReader::EmitPending(
    Node& node) noexcept {
    if (pendingIndex_ >= pending_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageInvalidState.Data());
    }

    node = std::move(pending_[pendingIndex_]);
    ++pendingIndex_;
    if (node.Kind() == NodeKind::EndOfDocument) {
        ended_ = true;
    }
    return node.Kind();
}

Base::Status NodeReader::Failure(
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

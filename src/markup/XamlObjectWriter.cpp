#include <Aero/Markup/XamlObjectWriter.hpp>

#include <utility>

namespace Aero::Markup {
namespace {

constexpr Base::StringView MessageSchemaNotReady(
    "XAML object writer requires a frozen schema context");
constexpr Base::StringView MessageUnknownType(
    "XAML object element does not resolve to a registered type");
constexpr Base::StringView MessageTypeNotConstructible(
    "XAML object element resolves to a non-constructible type");
constexpr Base::StringView MessageUnknownMember(
    "XAML member does not resolve on the target object type");
constexpr Base::StringView MessageInvalidAttachedMember(
    "XAML qualified member is not valid for this target object");
constexpr Base::StringView MessageUnsupportedMember(
    "XAML member has no supported object-writer adapter");
constexpr Base::StringView MessageInvalidValue(
    "XAML value conversion or member assignment failed");
constexpr Base::StringView MessageInvalidWriterState(
    "XAML node sequence is invalid for the object writer");
constexpr Base::StringView MessageMissingContentProperty(
    "XAML child object requires a registered content property");
constexpr Base::StringView MessageDuplicateMemberValue(
    "XAML member is assigned more than once");
constexpr Base::StringView MessageInitializationFailed(
    "XAML object initialization callback failed");
constexpr Base::StringView MessageUnexpectedText(
    "XAML text is not valid in the current object context");
constexpr Base::StringView MessageTypeMismatch(
    "XAML object or scalar value is not assignable to the target member");
constexpr Base::StringView MessageFactoryFailed(
    "XAML object factory failed");
constexpr Base::StringView MessageMissingMemberValue(
    "XAML member scope does not contain a value");
constexpr Base::StringView MessageMultipleRoots(
    "XAML document contains more than one root object");
constexpr Base::StringView MessageInvalidDirective(
    "XAML language directive is unsupported or used in an invalid context");
constexpr Base::StringView MessageDuplicateName(
    "x:Name is duplicated in the active XAML name scope");
constexpr Base::StringView MessageDuplicateResourceKey(
    "x:Key is duplicated in the active XAML resource scope");
constexpr Base::StringView MessageStaticResourceNotFound(
    "StaticResource key is not available; forward references are not supported");
constexpr Base::StringView MessageMissingResourceScope(
    "x:Key requires an enclosing XAML resource scope");
constexpr Base::StringView MessageNullNotAllowed(
    "x:Null is not valid for this XAML value or document root");
constexpr Base::StringView MessageInvalidMarkupExtension(
    "XAML markup-extension text is malformed or unsupported");
constexpr Base::StringView MessageNamespaceState(
    "XAML namespace declaration state is invalid");
constexpr Base::StringView MessageNameRegistrationFailed(
    "XAML name registration callback failed");
constexpr Base::StringView MessageResourceRegistrationFailed(
    "XAML resource registration callback failed");
constexpr Base::StringView MessageUnknownMarkupExtension(
    "XAML markup-extension type or provider is not registered");
constexpr Base::StringView MessageMarkupExtensionFailed(
    "XAML markup-extension value provider failed");

constexpr Base::StringView XmlPrefix("xml");
constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
constexpr Base::StringView DirectiveName("Name");
constexpr Base::StringView DirectiveKey("Key");
constexpr Base::StringView DirectiveNull("Null");
constexpr Base::StringView NullMarkup("x:Null");
constexpr Base::StringView StaticResourceMarkup("StaticResource");

Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
}

bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    while (begin < value.SizeBytes() && IsAsciiWhitespace(value[begin])) {
        ++begin;
    }
    std::uint32_t end = value.SizeBytes();
    while (end > begin && IsAsciiWhitespace(value[end - 1U])) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool IsValueType(
    const Core::TypeRegistry& types,
    Core::TypeId type) noexcept {
    const Core::TypeInfo* info = types.FindType(type);
    return info != nullptr &&
        HasTypeFlag(info->Flags(), Core::TypeFlags::ValueType);
}

} // namespace

XamlObjectWriter::XamlObjectWriter(
    XamlSchemaContext& schema,
    Core::IDiagnosticSink* diagnostics,
    Base::IAllocator* allocator) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      frames_(allocator_),
      created_(allocator_),
      assignments_(allocator_),
      nameScopes_(allocator_),
      resourceScopes_(allocator_),
      namespaceBindings_(allocator_),
      pendingNamespaces_(allocator_),
      committedNames_(allocator_),
      committedResources_(allocator_) {}

XamlObjectWriter::~XamlObjectWriter() noexcept {
    AbortTransaction();
}

Base::Result<Base::Ref<Base::Object>> XamlObjectWriter::Load(
    XamlNodeReader& reader) noexcept {
    if (loading_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML object writer does not support reentrant Load calls");
    }

    AbortTransaction();
    committedNames_.Clear();
    committedResources_.Clear();
    if (!schema_->IsFrozen()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageSchemaNotReady.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageSchemaNotReady,
            {});
    }

    loading_ = true;
    XamlNode node(allocator_);
    while (!ended_) {
        Base::Result<XamlNodeKind> readResult = reader.Read(node);
        if (!readResult) {
            const Base::Status status = readResult.GetStatus();
            AbortTransaction();
            loading_ = false;
            return status;
        }

        Base::Result<void> processResult = ProcessNode(node);
        if (!processResult) {
            const Base::Status status = processResult.GetStatus();
            AbortTransaction();
            loading_ = false;
            return status;
        }
    }

    if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||
        !namespaceBindings_.Empty()) {
        const Base::Status status = Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
        AbortTransaction();
        loading_ = false;
        return status;
    }

    CommitDocumentScopes();
    Base::Ref<Base::Object> result = std::move(root_);
    ClearTransaction();
    loading_ = false;
    return result;
}

void XamlObjectWriter::Reset() noexcept {
    AbortTransaction();
    committedNames_.Clear();
    committedResources_.Clear();
    loading_ = false;
}

Base::Result<void> XamlObjectWriter::ProcessNode(
    const XamlNode& node) noexcept {
    switch (node.Kind()) {
    case XamlNodeKind::NamespaceDeclaration:
        return QueueNamespaceDeclaration(node);
    case XamlNodeKind::StartObject:
        return StartObject(node);
    case XamlNodeKind::EndObject:
        return EndObject(node);
    case XamlNodeKind::StartMember:
        return StartMember(node);
    case XamlNodeKind::EndMember:
        return EndMember(node);
    case XamlNodeKind::Value:
        return WriteText(node);
    case XamlNodeKind::EndOfDocument:
        if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||
            !namespaceBindings_.Empty()) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        ended_ = true;
        return {};
    case XamlNodeKind::None:
        break;
    }

    return Failure(
        InvalidStateStatus(),
        XamlObjectWriterDiagnosticCodes::InvalidWriterState,
        MessageInvalidWriterState,
        node.Source());
}

Base::Result<void> XamlObjectWriter::QueueNamespaceDeclaration(
    const XamlNode& node) noexcept {
    if (node.NamespaceUri().Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNamespaceState.Data()),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    PendingNamespaceRecord record(allocator_);
    Base::Result<void> prefixResult = record.prefix.TryAssignUnchecked(
        node.NamespacePrefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = record.uri.TryAssignUnchecked(
        node.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    record.source = node.Source();
    return pendingNamespaces_.TryPushBack(std::move(record));
}

Base::Result<void> XamlObjectWriter::StartObject(
    const XamlNode& node) noexcept {
    if (frames_.Empty() && root_) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageMultipleRoots.Data()),
            XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
            MessageMultipleRoots,
            node.Source());
    }

    std::uint32_t bindingStart = InvalidIndex;
    Base::Result<void> namespaceResult =
        ActivatePendingNamespaces(bindingStart);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    if (!frames_.Empty() &&
        frames_.Back().kind == FrameKind::Object &&
        HasPropertyElementSyntax(node.Name())) {
        return StartPropertyElement(
            node,
            frames_.Size() - 1U,
            bindingStart);
    }

    if (IsXamlNullObject(node.Name())) {
        return StartNullObject(node, bindingStart);
    }

    Base::Result<const Core::TypeInfo*> typeResult = schema_->ResolveType(
        node.Name().NamespaceUri(),
        node.Name().LocalName());
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownType,
            MessageUnknownType,
            node.Source());
    }

    const Core::TypeInfo* type = typeResult.Value();
    Base::Result<Base::Ref<Base::Object>> createResult =
        schema_->CreateObject(type->Id());
    if (!createResult) {
        const bool nonConstructible =
            createResult.GetStatus().code == Base::ErrorCode::Unsupported;
        return Failure(
            createResult.GetStatus(),
            nonConstructible
                ? XamlObjectWriterDiagnosticCodes::TypeNotConstructible
                : XamlObjectWriterDiagnosticCodes::FactoryFailed,
            nonConstructible
                ? MessageTypeNotConstructible
                : MessageFactoryFailed,
            node.Source());
    }

    CreatedObjectRecord record(allocator_);
    record.object = std::move(createResult).Value();
    record.type = type->Id();
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appendObject =
        created_.TryPushBack(std::move(record));
    if (!appendObject) {
        return Failure(
            appendObject.GetStatus(),
            XamlObjectWriterDiagnosticCodes::FactoryFailed,
            MessageFactoryFailed,
            node.Source());
    }

    if (rootObjectIndex_ == InvalidIndex) {
        rootObjectIndex_ = objectIndex;
    }

    CreatedObjectRecord& stored = created_[objectIndex];
    stored.beginCalled = true;
    Base::Result<void> beginResult = schema_->BeginInit(
        stored.type,
        *stored.object);
    if (!beginResult) {
        return Failure(
            beginResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Object;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    Base::Result<void> scopeResult = CreateScopesForObject(
        objectIndex,
        frame,
        node.Source());
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }

    Base::Result<void> appendFrame = frames_.TryPushBack(frame);
    if (!appendFrame) {
        return Failure(
            appendFrame.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> XamlObjectWriter::StartNullObject(
    const XamlNode& node,
    std::uint32_t bindingStart) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Member &&
         frames_.Back().kind != FrameKind::Object)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::NullObject;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    Base::Result<void> appendResult = frames_.TryPushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> XamlObjectWriter::EndObject(
    const XamlNode& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::NullObject) {
        return CompleteNullObject(node);
    }
    if (frame.kind == FrameKind::Member) {
        if (!frame.propertyElement) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        if (frame.valuesWritten == 0U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        const std::uint32_t bindingStart = frame.namespaceBindingStart;
        frames_.PopBack();
        PopNamespaceBindings(bindingStart);
        return {};
    }
    if (frame.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    return CompleteObject(node);
}

Base::Result<void> XamlObjectWriter::StartMember(
    const XamlNode& node) noexcept {
    if (frames_.Empty() || frames_.Back().kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& objectFrame = frames_.Back();
    if (objectFrame.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    if (node.Name().NamespaceUri() == XamlLanguageNamespaceUri()) {
        if (IsXamlDirective(node.Name(), DirectiveName)) {
            return StartDirective(
                node,
                DirectiveKind::Name,
                objectFrame.objectIndex);
        }
        if (IsXamlDirective(node.Name(), DirectiveKey)) {
            return StartDirective(
                node,
                DirectiveKind::Key,
                objectFrame.objectIndex);
        }
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    Base::Result<XamlResolvedMember> memberResult = schema_->ResolveMember(
        created_[objectFrame.objectIndex].type,
        node.Name(),
        XamlMemberSyntax::Attribute);
    if (!memberResult) {
        const bool notFound =
            memberResult.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            memberResult.GetStatus(),
            notFound
                ? XamlObjectWriterDiagnosticCodes::UnknownMember
                : XamlObjectWriterDiagnosticCodes::InvalidAttachedMember,
            notFound ? MessageUnknownMember : MessageInvalidAttachedMember,
            node.Source());
    }

    const XamlResolvedMember member = memberResult.Value();
    if (member.kind != Core::MemberKind::Property ||
        schema_->FindMemberAdapter(member.id) == nullptr) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Member;
    frame.targetObjectIndex = objectFrame.objectIndex;
    frame.member = member;
    frame.source = node.Source();
    frame.propertyElement = false;
    Base::Result<void> appendResult = frames_.TryPushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> XamlObjectWriter::StartDirective(
    const XamlNode& node,
    DirectiveKind directive,
    std::uint32_t targetObjectIndex) noexcept {
    if (!node.IsFromAttribute() || targetObjectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    const CreatedObjectRecord& record = created_[targetObjectIndex];
    if ((directive == DirectiveKind::Name &&
         (!record.name.Empty() || record.nameRegistered)) ||
        (directive == DirectiveKind::Key &&
         (!record.key.Empty() || record.resourceRegistered))) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Directive;
    frame.directive = directive;
    frame.targetObjectIndex = targetObjectIndex;
    frame.source = node.Source();
    Base::Result<void> appendResult = frames_.TryPushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> XamlObjectWriter::EndMember(
    const XamlNode& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::Directive) {
        if (frame.valuesWritten != 1U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        frames_.PopBack();
        return {};
    }

    if (frame.kind != FrameKind::Member || frame.propertyElement) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frame.valuesWritten == 0U) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMissingMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::MissingMemberValue,
            MessageMissingMemberValue,
            node.Source());
    }
    frames_.PopBack();
    return {};
}

Base::Result<void> XamlObjectWriter::WriteText(
    const XamlNode& node) noexcept {
    if (!node.IsFromAttribute() && IsWhitespaceOnly(node.Value())) {
        return {};
    }
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::Directive) {
        return WriteDirectiveText(frame, node);
    }

    if (frame.kind == FrameKind::Member) {
        const XamlMemberAdapterRegistration* adapter =
            schema_->FindMemberAdapter(frame.member.id);
        const bool acceptsAnyValue = adapter != nullptr &&
            adapter->acceptsAnyValue;
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(),
            extensionName,
            argument);
        if (markup == MarkupValueKind::Invalid) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        if (markup == MarkupValueKind::Null) {
            if (IsValueType(schema_->Types(), frame.member.valueType) &&
                !acceptsAnyValue) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
            XamlValue value = XamlValue::NullObject(
                frame.member.valueType,
                allocator_);
            return WriteValueToMember(frame, std::move(value), node.Source());
        }
        if (markup == MarkupValueKind::StaticResource) {
            Base::Result<XamlResourceValue> resource = LookupResource(argument);
            if (!resource) {
                return Failure(
                    resource.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                    MessageStaticResourceNotFound,
                    node.Source());
            }
            XamlValue value = XamlValue::FromObject(
                resource.Value().type,
                resource.Value().object,
                allocator_);
            return WriteValueToMember(frame, std::move(value), node.Source());
        }

        if (markup == MarkupValueKind::Extension) {
            Base::Result<XamlValue> value = EvaluateMarkupExtension(
                frame.targetObjectIndex,
                frame.member,
                extensionName,
                argument,
                node.Source());
            if (!value) {
                return value.GetStatus();
            }
            return WriteValueToMember(
                frame,
                std::move(value).Value(),
                node.Source());
        }

        Base::Result<XamlValue> convertResult = schema_->ConvertText(
            frame.member.valueType,
            markup == MarkupValueKind::EscapedLiteral
                ? argument
                : node.Value());
        if (!convertResult) {
            return Failure(
                convertResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        return WriteValueToMember(
            frame,
            std::move(convertResult).Value(),
            node.Source());
    }

    if (frame.kind != FrameKind::Object ||
        frame.objectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Base::Result<XamlResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[frame.objectIndex].type);
    if (!contentResult) {
        return Failure(
            contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Base::StringView extensionName;
    Base::StringView argument;
    const MarkupValueKind markup = ParseMarkupValue(
        node.Value(),
        extensionName,
        argument);
    if (markup == MarkupValueKind::Invalid) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidMarkupExtension.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
            MessageInvalidMarkupExtension,
            node.Source());
    }
    if (markup == MarkupValueKind::Null) {
        if (IsValueType(
                schema_->Types(),
                contentResult.Value().valueType)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                node.Source());
        }
        XamlValue value = XamlValue::NullObject(
            contentResult.Value().valueType,
            allocator_);
        return WriteValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(value),
            node.Source());
    }
    if (markup == MarkupValueKind::StaticResource) {
        Base::Result<XamlResourceValue> resource = LookupResource(argument);
        if (!resource) {
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                MessageStaticResourceNotFound,
                node.Source());
        }
        XamlValue value = XamlValue::FromObject(
            resource.Value().type,
            resource.Value().object,
            allocator_);
        return WriteValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(value),
            node.Source());
    }

    if (markup == MarkupValueKind::Extension) {
        Base::Result<XamlValue> value = EvaluateMarkupExtension(
            frame.objectIndex,
            contentResult.Value(),
            extensionName,
            argument,
            node.Source());
        if (!value) {
            return value.GetStatus();
        }
        return WriteValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(value).Value(),
            node.Source());
    }

    Base::Result<XamlValue> convertResult = schema_->ConvertText(
        contentResult.Value().valueType,
        markup == MarkupValueKind::EscapedLiteral
            ? argument
            : node.Value());
    if (!convertResult) {
        return Failure(
            convertResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            node.Source());
    }
    return WriteValue(
        frame.objectIndex,
        contentResult.Value(),
        std::move(convertResult).Value(),
        node.Source());
}

Base::Result<void> XamlObjectWriter::WriteDirectiveText(
    Frame& frame,
    const XamlNode& node) noexcept {
    if (frame.targetObjectIndex >= created_.Size() ||
        frame.valuesWritten != 0U || !node.IsFromAttribute()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    CreatedObjectRecord& object = created_[frame.targetObjectIndex];
    if (frame.directive == DirectiveKind::Name) {
        if (!NameScope::IsValidName(node.Value())) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::Result<void> assignResult = object.name.TryAssign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
        Base::Result<void> registerResult = RegisterObjectName(
            frame.targetObjectIndex,
            node.Source());
        if (!registerResult) {
            return registerResult.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Key) {
        if (node.Value().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::Result<void> assignResult = object.key.TryAssign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
    } else {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    frame.valuesWritten = 1U;
    return {};
}

Base::Result<void> XamlObjectWriter::StartPropertyElement(
    const XamlNode& node,
    std::uint32_t targetFrameIndex,
    std::uint32_t bindingStart) noexcept {
    if (targetFrameIndex >= frames_.Size() ||
        frames_[targetFrameIndex].kind != FrameKind::Object ||
        frames_[targetFrameIndex].objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const std::uint32_t targetObjectIndex =
        frames_[targetFrameIndex].objectIndex;
    Base::Result<XamlResolvedMember> memberResult = schema_->ResolveMember(
        created_[targetObjectIndex].type,
        node.Name(),
        XamlMemberSyntax::PropertyElement);
    if (!memberResult) {
        const bool notFound =
            memberResult.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            memberResult.GetStatus(),
            notFound
                ? XamlObjectWriterDiagnosticCodes::UnknownMember
                : XamlObjectWriterDiagnosticCodes::InvalidAttachedMember,
            notFound ? MessageUnknownMember : MessageInvalidAttachedMember,
            node.Source());
    }

    const XamlResolvedMember member = memberResult.Value();
    if (member.kind != Core::MemberKind::Property ||
        schema_->FindMemberAdapter(member.id) == nullptr) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Member;
    frame.targetObjectIndex = targetObjectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.member = member;
    frame.source = node.Source();
    frame.propertyElement = true;
    Base::Result<void> appendResult = frames_.TryPushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> XamlObjectWriter::CompleteObject(
    const XamlNode& node) noexcept {
    if (frames_.Empty() || frames_.Back().kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame frame = frames_.Back();
    const std::uint32_t objectIndex = frame.objectIndex;
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    CreatedObjectRecord& record = created_[objectIndex];
    if (record.endCalled) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    if (!record.name.Empty() && !record.nameRegistered) {
        Base::Result<void> nameResult = RegisterObjectName(
            objectIndex,
            node.Source());
        if (!nameResult) {
            return nameResult.GetStatus();
        }
    }

    Base::Result<void> endResult = schema_->EndInit(
        record.type,
        *record.object);
    if (!endResult) {
        return Failure(
            endResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }
    record.endCalled = true;

    frames_.PopBack();
    PopNamespaceBindings(frame.namespaceBindingStart);

    Base::Result<bool> resourceResult = RegisterObjectResource(
        objectIndex,
        node.Source());
    if (!resourceResult) {
        return resourceResult.GetStatus();
    }
    if (resourceResult.Value()) {
        return {};
    }
    return WriteObjectToParent(objectIndex, node.Source());
}

Base::Result<void> XamlObjectWriter::CompleteNullObject(
    const XamlNode& node) noexcept {
    if (frames_.Empty() || frames_.Back().kind != FrameKind::NullObject) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    const std::uint32_t bindingStart =
        frames_.Back().namespaceBindingStart;
    frames_.PopBack();
    PopNamespaceBindings(bindingStart);
    return WriteNullToParent(node.Source());
}

Base::Result<void> XamlObjectWriter::WriteObjectToParent(
    std::uint32_t objectIndex,
    Core::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    if (frames_.Empty()) {
        if (root_) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageMultipleRoots.Data()),
                XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
                MessageMultipleRoots,
                source);
        }
        root_ = created_[objectIndex].object;
        return {};
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        XamlValue value = XamlValue::FromObject(
            created_[objectIndex].type,
            created_[objectIndex].object,
            allocator_);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    return WriteObjectToContent(
        parent.objectIndex,
        objectIndex,
        source);
}

Base::Result<void> XamlObjectWriter::WriteObjectToContent(
    std::uint32_t parentObjectIndex,
    std::uint32_t childObjectIndex,
    Core::SourceSpan source) noexcept {
    if (parentObjectIndex >= created_.Size() ||
        childObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Base::Result<XamlResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[parentObjectIndex].type);
    if (!contentResult) {
        return Failure(
            contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }

    XamlValue value = XamlValue::FromObject(
        created_[childObjectIndex].type,
        created_[childObjectIndex].object,
        allocator_);
    return WriteValue(
        parentObjectIndex,
        contentResult.Value(),
        std::move(value),
        source);
}

Base::Result<void> XamlObjectWriter::WriteNullToParent(
    Core::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        const XamlMemberAdapterRegistration* adapter =
            schema_->FindMemberAdapter(parent.member.id);
        const bool acceptsAnyValue = adapter != nullptr &&
            adapter->acceptsAnyValue;
        if (IsValueType(schema_->Types(), parent.member.valueType) &&
            !acceptsAnyValue) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                source);
        }
        XamlValue value = XamlValue::NullObject(
            parent.member.valueType,
            allocator_);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object ||
        parent.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Base::Result<XamlResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[parent.objectIndex].type);
    if (!contentResult ||
        IsValueType(schema_->Types(), contentResult.Value().valueType)) {
        return Failure(
            contentResult ? Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()) : contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    XamlValue value = XamlValue::NullObject(
        contentResult.Value().valueType,
        allocator_);
    return WriteValue(
        parent.objectIndex,
        contentResult.Value(),
        std::move(value),
        source);
}

Base::Result<void> XamlObjectWriter::WriteValueToMember(
    Frame& memberFrame,
    XamlValue&& value,
    Core::SourceSpan source) noexcept {
    Base::Result<void> result = WriteValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(value),
        source);
    if (!result) {
        return result.GetStatus();
    }
    if (memberFrame.valuesWritten == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++memberFrame.valuesWritten;
    return {};
}

Base::Result<void> XamlObjectWriter::WriteValue(
    std::uint32_t targetObjectIndex,
    const XamlResolvedMember& member,
    XamlValue&& value,
    Core::SourceSpan source) noexcept {
    if (targetObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const XamlMemberAdapterRegistration* adapter =
        schema_->FindMemberAdapter(member.id);
    if (adapter == nullptr ||
        (adapter->set == nullptr && adapter->setWithServices == nullptr)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            source);
    }

    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex,
        member.id);
    if (assignment != nullptr && assignment->count != 0U &&
        adapter->mode == XamlMemberWriteMode::SetOnce) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }

    if (assignment == nullptr) {
        Base::Result<void> appendResult = assignments_.TryPushBack({
            targetObjectIndex,
            member.id,
            0U});
        if (!appendResult) {
            return Failure(
                appendResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        assignment = &assignments_.Back();
    }

    const XamlServiceProvider services = BuildServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<void> setResult = schema_->SetMember(
        *created_[targetObjectIndex].object,
        created_[targetObjectIndex].type,
        member,
        value,
        &services);
    if (!setResult) {
        Core::DiagnosticCode code =
            XamlObjectWriterDiagnosticCodes::InvalidValue;
        Base::StringView message = MessageInvalidValue;
        if (setResult.GetStatus().code == Base::ErrorCode::Unsupported) {
            code = XamlObjectWriterDiagnosticCodes::UnsupportedMember;
            message = MessageUnsupportedMember;
        } else if (setResult.GetStatus().code ==
            Base::ErrorCode::InvalidArgument) {
            code = XamlObjectWriterDiagnosticCodes::TypeMismatch;
            message = MessageTypeMismatch;
        }
        return Failure(setResult.GetStatus(), code, message, source);
    }

    if (assignment->count == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++assignment->count;
    return {};
}

Base::Result<void> XamlObjectWriter::RegisterObjectName(
    std::uint32_t objectIndex,
    Core::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.name.Empty() || object.nameRegistered) {
        return {};
    }

    const std::uint32_t scopeIndex =
        FindNameScopeIndexForObject(objectIndex);
    if (scopeIndex == InvalidIndex || scopeIndex >= nameScopes_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    NameScopeRecord& scope = nameScopes_[scopeIndex];
    Base::Result<void> localResult = scope.names.TryRegister(
        object.name.View(),
        *object.object);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateName
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateName : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->RegisterName(
        owner.type,
        *owner.object,
        object.name.View(),
        *object.object);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NameRegistrationFailed,
            MessageNameRegistrationFailed,
            source);
    }

    object.nameRegistered = true;
    return {};
}

Base::Result<bool> XamlObjectWriter::RegisterObjectResource(
    std::uint32_t objectIndex,
    Core::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.key.Empty()) {
        return false;
    }
    if (object.resourceRegistered) {
        return true;
    }
    if (!frames_.Empty() && frames_.Back().kind == FrameKind::Member) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageMissingResourceScope.Data()),
            XamlObjectWriterDiagnosticCodes::MissingResourceScope,
            MessageMissingResourceScope,
            source);
    }

    const std::uint32_t scopeIndex = FindResourceScopeIndexForParent();
    if (scopeIndex == InvalidIndex || scopeIndex >= resourceScopes_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMissingResourceScope.Data()),
            XamlObjectWriterDiagnosticCodes::MissingResourceScope,
            MessageMissingResourceScope,
            source);
    }

    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];
    Base::Result<void> localResult = scope.resources.TryAdd(
        object.key.View(),
        object.type,
        object.object,
        source);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateResourceKey
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateResourceKey : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->AddResource(
        owner.type,
        *owner.object,
        object.key.View(),
        object.type,
        object.object);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    object.resourceRegistered = true;
    return true;
}

Base::Result<XamlResourceValue> XamlObjectWriter::LookupResource(
    Base::StringView key) const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.resourceScopeIndex == InvalidIndex ||
            frame.resourceScopeIndex >= resourceScopes_.Size()) {
            continue;
        }
        Base::Result<XamlResourceValue> value =
            resourceScopes_[frame.resourceScopeIndex].resources.Lookup(key);
        if (value) {
            return value;
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        MessageStaticResourceNotFound.Data());
}

Base::Result<void> XamlObjectWriter::CreateScopesForObject(
    std::uint32_t objectIndex,
    Frame& frame,
    Core::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const bool documentRoot = objectIndex == rootObjectIndex_;
    if (documentRoot || schema_->CreatesNameScope(created_[objectIndex].type)) {
        NameScopeRecord scope(allocator_);
        scope.ownerObjectIndex = objectIndex;
        const std::uint32_t index = nameScopes_.Size();
        Base::Result<void> appendResult =
            nameScopes_.TryPushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.nameScopeIndex = index;
        if (documentRoot) {
            documentNameScopeIndex_ = index;
        }
    }

    if (documentRoot ||
        schema_->CreatesResourceScope(created_[objectIndex].type)) {
        ResourceScopeRecord scope(allocator_);
        scope.ownerObjectIndex = objectIndex;
        const std::uint32_t index = resourceScopes_.Size();
        Base::Result<void> appendResult =
            resourceScopes_.TryPushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.resourceScopeIndex = index;
        if (documentRoot) {
            documentResourceScopeIndex_ = index;
        }
    }
    return {};
}

Base::Result<void> XamlObjectWriter::ActivatePendingNamespaces(
    std::uint32_t& bindingStart) noexcept {
    bindingStart = namespaceBindings_.Size();
    for (PendingNamespaceRecord& pending : pendingNamespaces_) {
        NamespaceBindingRecord binding(allocator_);
        Base::Result<void> prefixResult = binding.prefix.TryAssignUnchecked(
            pending.prefix.View());
        if (!prefixResult) {
            return prefixResult.GetStatus();
        }
        Base::Result<void> uriResult = binding.uri.TryAssignUnchecked(
            pending.uri.View());
        if (!uriResult) {
            return uriResult.GetStatus();
        }
        Base::Result<void> appendResult =
            namespaceBindings_.TryPushBack(std::move(binding));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
    }
    pendingNamespaces_.Clear();
    return {};
}

void XamlObjectWriter::PopNamespaceBindings(
    std::uint32_t bindingStart) noexcept {
    if (bindingStart == InvalidIndex) {
        return;
    }
    while (namespaceBindings_.Size() > bindingStart) {
        namespaceBindings_.PopBack();
    }
}

Base::Result<Base::StringView> XamlObjectWriter::LookupNamespace(
    Base::StringView prefix) const noexcept {
    if (prefix == XmlPrefix) {
        return XmlNamespaceUri;
    }
    for (std::uint32_t index = namespaceBindings_.Size();
         index > 0U;
         --index) {
        const NamespaceBindingRecord& binding =
            namespaceBindings_[index - 1U];
        if (binding.prefix.View() == prefix) {
            return binding.uri.View();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML namespace prefix is not bound in the active scope");
}

XamlServiceProvider XamlObjectWriter::BuildServices(
    std::uint32_t targetObjectIndex,
    const XamlResolvedMember& member,
    Core::SourceSpan source) noexcept {
    XamlServiceProvider services;
    services.schema = schema_;
    if (targetObjectIndex < created_.Size()) {
        services.targetObject = created_[targetObjectIndex].object.Get();
        services.targetObjectType = created_[targetObjectIndex].type;
    }
    services.targetMember = member.id;
    services.targetValueType = member.valueType;
    if (rootObjectIndex_ < created_.Size()) {
        services.rootObject = created_[rootObjectIndex_].object.Get();
    }
    services.source = source;
    services.nameScope = FindActiveNameScope();
    services.namespaces = XamlNamespaceScope(
        &XamlObjectWriter::NamespaceLookupCallback,
        this);
    services.resources = XamlResourceResolver(
        &XamlObjectWriter::ResourceLookupCallback,
        this);
    return services;
}

const NameScope* XamlObjectWriter::FindActiveNameScope() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.nameScopeIndex != InvalidIndex &&
            frame.nameScopeIndex < nameScopes_.Size()) {
            return &nameScopes_[frame.nameScopeIndex].names;
        }
    }
    return nullptr;
}

std::uint32_t XamlObjectWriter::FindNameScopeIndexForObject(
    std::uint32_t objectIndex) const noexcept {
    const std::uint32_t objectFrame = FindObjectFrameIndex(objectIndex);
    if (objectFrame == InvalidIndex) {
        return InvalidIndex;
    }
    for (std::uint32_t index = objectFrame + 1U; index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.nameScopeIndex != InvalidIndex) {
            return frame.nameScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t XamlObjectWriter::FindResourceScopeIndexForParent() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.resourceScopeIndex != InvalidIndex) {
            return frame.resourceScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t XamlObjectWriter::FindObjectFrameIndex(
    std::uint32_t objectIndex) const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.objectIndex == objectIndex) {
            return index - 1U;
        }
    }
    return InvalidIndex;
}

XamlObjectWriter::MarkupValueKind XamlObjectWriter::ParseMarkupValue(
    Base::StringView text,
    Base::StringView& extensionName,
    Base::StringView& argument) const noexcept {
    extensionName = {};
    argument = {};
    const Base::StringView value = TrimAscii(text);
    if (value.Empty() || value[0] != '{') {
        return MarkupValueKind::Literal;
    }
    if (value.SizeBytes() >= 2U && value[1] == '}') {
        argument = value.Substr(2U, value.SizeBytes() - 2U);
        return MarkupValueKind::EscapedLiteral;
    }
    if (value.SizeBytes() < 2U ||
        value[value.SizeBytes() - 1U] != '}') {
        return MarkupValueKind::Invalid;
    }

    const Base::StringView inner = TrimAscii(value.Substr(
        1U,
        value.SizeBytes() - 2U));
    if (inner == NullMarkup) {
        return MarkupValueKind::Null;
    }

    for (char character : inner) {
        if (character == '{' || character == '}') {
            return MarkupValueKind::Invalid;
        }
    }

    std::uint32_t nameEnd = 0U;
    while (nameEnd < inner.SizeBytes() &&
           !IsAsciiWhitespace(inner[nameEnd]) && inner[nameEnd] != ',') {
        ++nameEnd;
    }
    if (nameEnd == 0U) {
        return MarkupValueKind::Invalid;
    }
    extensionName = inner.Substr(0U, nameEnd);

    argument = TrimAscii(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));
    if (!argument.Empty() && argument[0] == ',') {
        argument = TrimAscii(argument.Substr(1U, argument.SizeBytes() - 1U));
    }
    if (extensionName == StaticResourceMarkup) {
        if (argument.Empty()) {
            return MarkupValueKind::Invalid;
        }
        return MarkupValueKind::StaticResource;
    }
    return MarkupValueKind::Extension;
}

Base::Result<XamlValue> XamlObjectWriter::EvaluateMarkupExtension(
    std::uint32_t targetObjectIndex,
    const XamlResolvedMember& member,
    Base::StringView extensionName,
    Base::StringView arguments,
    Core::SourceSpan source) noexcept {
    std::uint32_t colon = extensionName.SizeBytes();
    for (std::uint32_t index = 0U;
         index < extensionName.SizeBytes();
         ++index) {
        if (extensionName[index] != ':') {
            continue;
        }
        if (colon != extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        colon = index;
    }

    Base::StringView prefix;
    Base::StringView localName = extensionName;
    if (colon != extensionName.SizeBytes()) {
        if (colon == 0U || colon + 1U >= extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        prefix = extensionName.Substr(0U, colon);
        localName = extensionName.Substr(
            colon + 1U,
            extensionName.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> namespaceResult = LookupNamespace(prefix);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }
    Base::Result<const Core::TypeInfo*> typeResult = schema_->ResolveType(
        namespaceResult.Value(),
        localName);
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }

    const XamlServiceProvider services = BuildServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<XamlValue> provided =
        schema_->ProvideMarkupExtensionValue(
            typeResult.Value()->Id(),
            arguments,
            services);
    if (!provided) {
        const bool missing =
            provided.GetStatus().code == Base::ErrorCode::Unsupported ||
            provided.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            provided.GetStatus(),
            missing
                ? XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension
                : XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
            missing
                ? MessageUnknownMarkupExtension
                : MessageMarkupExtensionFailed,
            source);
    }
    return provided;
}

bool XamlObjectWriter::IsXamlDirective(
    const XamlQualifiedName& name,
    Base::StringView localName) const noexcept {
    return name.NamespaceUri() == XamlLanguageNamespaceUri() &&
        name.LocalName() == localName;
}

bool XamlObjectWriter::IsXamlNullObject(
    const XamlQualifiedName& name) const noexcept {
    return IsXamlDirective(name, DirectiveNull);
}

bool XamlObjectWriter::HasPropertyElementSyntax(
    const XamlQualifiedName& name) const noexcept {
    for (char character : name.LocalName()) {
        if (character == '.') {
            return true;
        }
    }
    return false;
}

bool XamlObjectWriter::IsWhitespaceOnly(
    Base::StringView value) const noexcept {
    for (char character : value) {
        if (!IsAsciiWhitespace(character)) {
            return false;
        }
    }
    return true;
}

XamlObjectWriter::AssignmentRecord* XamlObjectWriter::FindAssignment(
    std::uint32_t objectIndex,
    Core::MemberId member) noexcept {
    for (AssignmentRecord& assignment : assignments_) {
        if (assignment.objectIndex == objectIndex &&
            assignment.member == member) {
            return &assignment;
        }
    }
    return nullptr;
}

void XamlObjectWriter::CommitDocumentScopes() noexcept {
    committedNames_.Clear();
    committedResources_.Clear();
    if (documentNameScopeIndex_ < nameScopes_.Size()) {
        committedNames_ = std::move(
            nameScopes_[documentNameScopeIndex_].names);
    }
    if (documentResourceScopeIndex_ < resourceScopes_.Size()) {
        committedResources_ = std::move(
            resourceScopes_[documentResourceScopeIndex_].resources);
    }
}

void XamlObjectWriter::AbortTransaction() noexcept {
    root_.Reset();
    for (std::uint32_t index = created_.Size(); index > 0U; --index) {
        CreatedObjectRecord& record = created_[index - 1U];
        if (record.beginCalled && record.object) {
            schema_->AbortInit(record.type, *record.object);
        }
    }
    ClearTransaction();
}

void XamlObjectWriter::ClearTransaction() noexcept {
    frames_.Clear();
    assignments_.Clear();
    nameScopes_.Clear();
    resourceScopes_.Clear();
    namespaceBindings_.Clear();
    pendingNamespaces_.Clear();
    created_.Clear();
    root_.Reset();
    rootObjectIndex_ = InvalidIndex;
    documentNameScopeIndex_ = InvalidIndex;
    documentResourceScopeIndex_ = InvalidIndex;
    ended_ = false;
}

Base::Status XamlObjectWriter::Failure(
    Base::Status status,
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
            Core::InvalidMemberId,
            allocator_);
        if (!item) {
            return item.GetStatus();
        }
        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }
    return status;
}

Base::Result<Base::StringView> XamlObjectWriter::NamespaceLookupCallback(
    void* context,
    Base::StringView prefix) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageNamespaceState.Data());
    }
    return static_cast<XamlObjectWriter*>(context)->LookupNamespace(prefix);
}

Base::Result<XamlResourceValue> XamlObjectWriter::ResourceLookupCallback(
    void* context,
    Base::StringView key) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageStaticResourceNotFound.Data());
    }
    return static_cast<XamlObjectWriter*>(context)->LookupResource(key);
}

} // namespace Aero::Markup

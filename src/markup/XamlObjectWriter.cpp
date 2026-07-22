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

Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
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
      assignments_(allocator_) {}

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

    if (!frames_.Empty() || !root_) {
        const Base::Status status = Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
        AbortTransaction();
        loading_ = false;
        return status;
    }

    Base::Ref<Base::Object> result = std::move(root_);
    frames_.Clear();
    assignments_.Clear();
    created_.Clear();
    ended_ = false;
    loading_ = false;
    return result;
}

void XamlObjectWriter::Reset() noexcept {
    AbortTransaction();
    loading_ = false;
}

Base::Result<void> XamlObjectWriter::ProcessNode(
    const XamlNode& node) noexcept {
    switch (node.Kind()) {
    case XamlNodeKind::NamespaceDeclaration:
        return {};
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
        if (!frames_.Empty() || !root_) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        ended_ = true;
        return {};
    case XamlNodeKind::None:
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    return Failure(
        InvalidStateStatus(),
        XamlObjectWriterDiagnosticCodes::InvalidWriterState,
        MessageInvalidWriterState,
        node.Source());
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

    if (!frames_.Empty() &&
        frames_.Back().kind == FrameKind::Object &&
        HasPropertyElementSyntax(node.Name())) {
        return StartPropertyElement(node, frames_.Size() - 1U);
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
            nonConstructible ? MessageTypeNotConstructible : MessageFactoryFailed,
            node.Source());
    }

    CreatedObjectRecord record;
    record.object = std::move(createResult).Value();
    record.type = type->Id();
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appendObject = created_.TryPushBack(std::move(record));
    if (!appendObject) {
        return Failure(
            appendObject.GetStatus(),
            XamlObjectWriterDiagnosticCodes::FactoryFailed,
            MessageFactoryFailed,
            node.Source());
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
    frame.source = node.Source();
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
        frames_.PopBack();
        return {};
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

Base::Result<void> XamlObjectWriter::EndMember(
    const XamlNode& node) noexcept {
    if (frames_.Empty() ||
        frames_.Back().kind != FrameKind::Member ||
        frames_.Back().propertyElement) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frames_.Back().valuesWritten == 0U) {
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
    if (frame.kind == FrameKind::Member) {
        Base::Result<XamlValue> convertResult = schema_->ConvertText(
            frame.member.valueType,
            node.Value());
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

    if (frame.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
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

    Base::Result<XamlValue> convertResult = schema_->ConvertText(
        contentResult.Value().valueType,
        node.Value());
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

Base::Result<void> XamlObjectWriter::StartPropertyElement(
    const XamlNode& node,
    std::uint32_t targetFrameIndex) noexcept {
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

    const std::uint32_t objectIndex = frames_.Back().objectIndex;
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
    return WriteObjectToParent(objectIndex, node.Source());
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
    if (adapter == nullptr || adapter->set == nullptr) {
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
    if (assignment != nullptr &&
        assignment->count != 0U &&
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

    Base::Result<void> setResult = schema_->SetMember(
        *created_[targetObjectIndex].object,
        created_[targetObjectIndex].type,
        member,
        value);
    if (!setResult) {
        Core::DiagnosticCode code = XamlObjectWriterDiagnosticCodes::InvalidValue;
        Base::StringView message = MessageInvalidValue;
        if (setResult.GetStatus().code == Base::ErrorCode::Unsupported) {
            code = XamlObjectWriterDiagnosticCodes::UnsupportedMember;
            message = MessageUnsupportedMember;
        } else if (setResult.GetStatus().code == Base::ErrorCode::InvalidArgument) {
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

std::uint32_t XamlObjectWriter::CurrentObjectFrameIndex() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        if (frames_[index - 1U].kind == FrameKind::Object) {
            return index - 1U;
        }
    }
    return InvalidIndex;
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

bool XamlObjectWriter::HasPropertyElementSyntax(
    const XamlQualifiedName& name) const noexcept {
    for (char character : name.LocalName()) {
        if (character == '.') {
            return true;
        }
    }
    return false;
}

bool XamlObjectWriter::IsWhitespaceOnly(Base::StringView value) const noexcept {
    for (char character : value) {
        if (character != ' ' && character != '\t' &&
            character != '\r' && character != '\n') {
            return false;
        }
    }
    return true;
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
    created_.Clear();
    root_.Reset();
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

} // namespace Aero::Markup

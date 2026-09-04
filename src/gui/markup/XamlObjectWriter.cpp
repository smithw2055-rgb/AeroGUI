#include "gui/markup/XamlObjectWriterInternal.hpp"

// Writer core / node stack. Property writes, markup-extension evaluation,
// and name/resource deferrals live in sibling translation units.

// ===== Scopes =====

#include <Aero/Markup/XamlReader.hpp>

// Name and resource scope implementation.

namespace Aero::Markup {
namespace {

constexpr const char* MessageNamespaceUnavailable =
    "XAML namespace scope is not available";
constexpr const char* MessageResourceResolverUnavailable =
    "XAML resource resolver is not available";

} // namespace

Base::Result<Base::StringView> NamespaceScope::Lookup(
    Base::StringView prefix) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageNamespaceUnavailable);
    }
    return lookup_(context_, prefix);
}

Base::Result<Aero::ResourceValue> ResourceResolver::Lookup(
    Base::StringView key) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageResourceResolverUnavailable);
    }
    return lookup_(context_, key);
}

} // namespace Aero::Markup

namespace Aero::Markup {
class NodeCursor {
public:
    virtual ~NodeCursor() = default;
    virtual Base::Result<const Node*> Read(
        Node& scratch) noexcept = 0;
};

namespace {

class StreamingXamlNodeCursor : public NodeCursor {
public:
    explicit StreamingXamlNodeCursor(
        NodeReader& reader) noexcept
        : reader_(&reader) {}

    Base::Result<const Node*> Read(
        Node& scratch) noexcept override {
        Base::Result<NodeKind> read =
            reader_->Read(scratch);
        return read
            ? Base::Result<const Node*>(&scratch)
            : Base::Result<const Node*>(
                  read.GetStatus());
    }

private:
    NodeReader* reader_ = nullptr;
};

class CompiledXamlNodeCursor : public NodeCursor {
public:
    explicit CompiledXamlNodeCursor(
        const CompiledDocument& document) noexcept
        : nodes_(document.Nodes()) {}

    Base::Result<const Node*> Read(
        Node&) noexcept override {
        if (index_ >= nodes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node stream ended unexpectedly");
        }
        return &nodes_[index_++];
    }

private:
    Base::Span<const Node> nodes_;
    std::uint32_t index_ = 0U;
};

} // namespace

ObjectBuilder::ObjectBuilder(
    ::Aero::Markup::Schema& schema,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics),
      frames_(),
      created_(),
      assignments_(),
      deferredStaticResources_(),
      nameScopes_(),
      resourceScopes_(),
      serviceResourceChain_(),
      namespaceBindings_(),
      pendingNamespaces_(),
      committedNames_(),
      committedResources_(),
      resultVisualContent_() {}

ObjectBuilder::~ObjectBuilder() noexcept {
    AbortTransaction();
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    NodeReader& reader) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = nullptr;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadReaderCore(reader);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    NodeReader& reader,
    const LoadState& context) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = &context;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadReaderCore(reader);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    const CompiledDocument& document) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = nullptr;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledCore(document);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    const CompiledDocument& document,
    const LoadState& context) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = &context;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledCore(document);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::CompleteLoad(
    Base::Result<Base::Ref<Base::Object>> loaded) noexcept {
    if (!loaded) return loaded.GetStatus();
    LoaderResult result;
    result.root = std::move(loaded).Value();
    result.metadata = schema_->Metadata();
    result.names = std::move(committedNames_);
    result.resources = std::move(committedResources_);
    result.visualContent = std::move(resultVisualContent_);
    result.effects.Items() = std::move(extensionEffects_);
    result.hasDeferredStaticResources = hasDeferredStaticResources_;
    if (loadContext_ != nullptr) {
        result.runtimeLifetime = loadContext_->effectLifetime;
    }
    if (!deferredContent_.Empty()) {
        result.Clear();
        AbortTransaction();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred XAML content was not finalized");
    }
    if (loadContext_ != nullptr &&
        loadContext_->finalize != nullptr) {
        Base::Result<void> finalized =
            loadContext_->finalize(
                result,
                loadContext_->finalizeContext);
        if (!finalized) {
            const Base::Status status =
                finalized.GetStatus();
            result.Clear();
            AbortTransaction();
            return status;
        }
    }
    // Source-backed merged dictionaries are committed by the loader finalizer.
    // Resolve queued StaticResource references only after that transaction so
    // sibling theme dictionaries participate in WPF resource lookup order.
    if (hasDeferredStaticResources_) {
        Base::Result<void> resolved =
            ResolveDeferredStaticResources();
        if (!resolved) {
            const Base::Status status = resolved.GetStatus();
            result.Clear();
            AbortTransaction();
            return status;
        }
    }
    {
        Base::Result<void> finalizedStyles = FinalizeDeferredStyles();
        if (!finalizedStyles) {
            const Base::Status status = finalizedStyles.GetStatus();
            result.Clear();
            AbortTransaction();
            return status;
        }
    }
    result.hasDeferredStaticResources =
        hasDeferredStaticResources_;
    Base::Result<void> prepared =
        result.effects.Prepare(result.names);
    if (!prepared) {
        const Base::Status status = prepared.GetStatus();
        result.Clear();
        AbortTransaction();
        return status;
    }
    ClearTransaction();
    return result;
}


Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadReaderCore(
    NodeReader& reader) noexcept {
    StreamingXamlNodeCursor cursor(reader);
    return LoadCursorCore(cursor);
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadCursorCore(
    NodeCursor& cursor) noexcept {
    if (consumed_ || loading_) return SessionConsumedStatus();
    consumed_ = true;

    AbortTransaction();
    committedNames_.Clear();
    committedResources_.Clear();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
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
    Node node;
    while (!ended_) {
        Base::Result<const Node*> readResult =
            cursor.Read(node);
        if (!readResult) {
            const Base::Status status = readResult.GetStatus();
            AbortTransaction();
            loading_ = false;
            return status;
        }

        const Node* current = readResult.Value();
        if (current == nullptr) {
            const Base::Status status = Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                {});
            AbortTransaction();
            loading_ = false;
            return status;
        }
        if (loadContext_ != nullptr &&
            loadContext_->recordingNodes != nullptr) {
            Base::Result<Node> cloned = Node::Clone(*current);
            if (!cloned) {
                const Base::Status status = cloned.GetStatus();
                AbortTransaction();
                loading_ = false;
                return status;
            }
            Base::Result<void> recorded =
                loadContext_->recordingNodes->PushBack(
                    std::move(cloned).Value());
            if (!recorded) {
                const Base::Status status = recorded.GetStatus();
                AbortTransaction();
                loading_ = false;
                return status;
            }
        }
        Base::Result<void> processResult =
            ProcessNode(*current);
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
    loading_ = false;
    return result;
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadCompiledCore(
    const CompiledDocument& document) noexcept {
    if (consumed_ || loading_) return SessionConsumedStatus();
    if (!schema_->IsFrozen() || !document.IsValid()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageSchemaNotReady.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageSchemaNotReady,
            {});
    }
    Base::Result<void> compatible =
        ValidateCompiledCacheIdentity(
            document.Identity(), schema_->Domain());
    if (!compatible) return compatible.GetStatus();

    CompiledXamlNodeCursor cursor(document);
    return LoadCursorCore(cursor);
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::CreateObject(
    Meta::TypeId type) const noexcept {
    if (loadContext_ != nullptr &&
        created_.Size() >= loadContext_->maxObjects) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML object count exceeds configured limits");
    }
    if (loadContext_ != nullptr &&
        loadContext_->existingRoot &&
        rootObjectIndex_ == InvalidIndex &&
        frames_.Empty()) {
        const Meta::TypeId actual =
            loadContext_->existingRoot->RuntimeType();
        if (actual == Meta::InvalidTypeId ||
            !schema_->Types().IsAssignableFrom(type, actual)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent root type is incompatible with XAML root type");
        }
        return loadContext_->existingRoot;
    }
    return schema_->CreateObject(type);
}

Base::Result<void> ObjectBuilder::ProcessNode(
    const Node& node) noexcept {
    switch (node.Kind()) {
    case NodeKind::NamespaceDeclaration:
        return QueueNamespaceDeclaration(node);
    case NodeKind::StartObject:
        return StartObject(node);
    case NodeKind::EndObject:
        return EndObject(node);
    case NodeKind::StartMember:
        return StartMember(node);
    case NodeKind::EndMember:
        return EndMember(node);
    case NodeKind::Value:
        return WriteText(node);
    case NodeKind::EndOfDocument:
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
    case NodeKind::None:
        break;
    }

    return Failure(
        InvalidStateStatus(),
        XamlObjectWriterDiagnosticCodes::InvalidWriterState,
        MessageInvalidWriterState,
        node.Source());
}

Base::Result<void> ObjectBuilder::QueueNamespaceDeclaration(
    const Node& node) noexcept {
    if (node.NamespaceUri().Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNamespaceState.Data()),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    PendingNamespaceRecord record;
    Base::Result<void> prefixResult = record.prefix.AssignUnchecked(
        node.NamespacePrefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = record.uri.AssignUnchecked(
        node.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    record.source = node.Source();
    return pendingNamespaces_.PushBack(std::move(record));
}

Base::Result<void> ObjectBuilder::StartObject(
    const Node& node) noexcept {
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
        (node.CompiledMemberId() != Meta::InvalidMemberId ||
         HasPropertyElementSyntax(node.Name()))) {
        return StartPropertyElement(
            node,
            frames_.Size() - 1U,
            bindingStart);
    }

    if (IsXamlNullObject(node.Name())) {
        return StartNullObject(node, bindingStart);
    }

    Meta::TypeId typeId = Meta::InvalidTypeId;
    Meta::TypeFlags typeFlags = Meta::TypeFlags::None;
    const CompiledTypeBinding* compiledType = nullptr;
    Base::Status typeStatus;
    if (node.HasCompiledTypeBinding()) {
        const CompiledTypeBinding& binding = node.CompiledType();
        compiledType = &binding;
        typeId = binding.id;
        typeFlags = binding.flags;
    } else if (node.CompiledTypeId() != Meta::InvalidTypeId) {
        const Meta::TypeInfo* type =
            schema_->Types().FindType(node.CompiledTypeId());
        if (type != nullptr) {
            typeId = type->Id();
            typeFlags = type->Flags();
        } else {
            typeStatus = Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "AXB2 type id is absent from the frozen schema");
        }
    } else {
        Base::Result<const Meta::TypeInfo*> typeResult =
            schema_->ResolveType(
                node.Name().NamespaceUri(),
                node.Name().LocalName());
        if (typeResult) {
            typeId = typeResult.Value()->Id();
            typeFlags = typeResult.Value()->Flags();
        } else {
            typeStatus = typeResult.GetStatus();
        }
    }
    if (typeId == Meta::InvalidTypeId) {
        return Failure(
            typeStatus,
            XamlObjectWriterDiagnosticCodes::UnknownType,
            MessageUnknownType,
            node.Source());
    }

    if (HasTypeFlag(typeFlags, Meta::TypeFlags::ValueType)) {
        return StartValueObject(node, bindingStart, typeId);
    }
    Base::Result<Base::Ref<Base::Object>> createResult =
        CreateObject(typeId);
    if (!createResult) {
        const bool nonConstructible =
            createResult.GetStatus().code == Base::ErrorCode::Unsupported;
        // WPF permits abstract object types with a text converter to be used
        // as value elements. ImageSource is the canonical example:
        // <ImageSource>Images/Atlas.png</ImageSource> materializes a
        // BitmapImage through the registered ImageSource converter. Defer
        // construction until the element text is seen; conversion will still
        // reject abstract types that have no converter.
        if (nonConstructible &&
            HasTypeFlag(typeFlags, Meta::TypeFlags::Abstract) &&
            !frames_.Empty()) {
            return StartValueObject(node, bindingStart, typeId);
        }
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

    CreatedObjectRecord record;
    record.object = std::move(createResult).Value();
    record.type = typeId;
    if (loadContext_ != nullptr && loadContext_->existingRoot &&
        rootObjectIndex_ == InvalidIndex && frames_.Empty() &&
        record.object.Get() == loadContext_->existingRoot.Get()) {
        const Meta::TypeId runtimeType = record.object->RuntimeType();
        if (runtimeType == Meta::InvalidTypeId) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Existing XAML root has no runtime type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        record.type = runtimeType;
    }
    if (compiledType != nullptr &&
        compiledType->HasContentMember()) {
        record.contentMember =
            ResolveCompiledMember(compiledType->contentMember);
        record.contentPolicy =
            ResolveCompiledMemberPolicy(
                compiledType->contentMember);
        record.hasContentMember = true;
        record.contentValueTypeIsObject =
            compiledType->contentMember.ValueTypeIsObject();
        record.contentValueTypeIsValueType =
            compiledType->contentMember.ValueTypeIsValueType();
    } else if (schema_ != nullptr) {
        Base::Result<ResolvedMember> content =
            schema_->ResolveContentMember(typeId);
        if (content) {
            record.contentMember = content.Value();
            record.contentPolicy =
                schema_->ResolveMemberWritePolicy(
                    record.contentMember);
            record.hasContentMember = true;
            if (const Meta::TypeInfo* valueType =
                    schema_->Types().FindType(
                        record.contentMember.valueType)) {
                record.contentValueTypeIsObject =
                    valueType->Kind() ==
                        Meta::MetadataTypeKind::Object;
                record.contentValueTypeIsValueType =
                    valueType->Kind() !=
                        Meta::MetadataTypeKind::Object &&
                    HasTypeFlag(
                        valueType->Flags(),
                        Meta::TypeFlags::ValueType);
            }
        }
    }
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appendObject =
        created_.PushBack(std::move(record));
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

    Base::Result<void> appendFrame = frames_.PushBack(frame);
    if (!appendFrame) {
        return Failure(
            appendFrame.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::StartValueObject(
    const Node& node,
    std::uint32_t bindingStart,
    Meta::TypeId type) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::Member)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            node.Source());
    }

    CreatedObjectRecord record;
    record.type = type;
    record.valueElement = true;
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appended = created_.PushBack(
        std::move(record));
    if (!appended) return appended.GetStatus();

    Frame frame;
    frame.kind = FrameKind::ValueObject;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    appended = frames_.PushBack(frame);
    if (!appended) return appended.GetStatus();
    return {};
}

Base::Result<void> ObjectBuilder::StartNullObject(
    const Node& node,
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
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> ObjectBuilder::EndObject(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueObject) {
        return CompleteValueObject(node);
    }
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
            const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    frame.member.valueType);
            // WPF permits an empty property element for reference-valued
            // properties. It means "leave the property's current/default
            // value in place", which is required by empty
            // Application.Resources declarations.
            if (valueType == nullptr ||
                valueType->Kind() !=
                    Meta::MetadataTypeKind::Object) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageMissingMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                    MessageMissingMemberValue,
                    node.Source());
            }
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

Base::Result<void> ObjectBuilder::StartMember(
    const Node& node) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::ValueObject)) {
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

    if (objectFrame.kind == FrameKind::ValueObject) {
        if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
            if (IsXamlDirective(node.Name(), DirectiveKey)) {
                return StartDirective(
                    node, DirectiveKind::Key, objectFrame.objectIndex);
            }
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        if (!node.IsFromAttribute() ||
            node.Name().LocalName() != Base::StringView("Value")) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    MessageUnknownMember.Data()),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                node.Source());
        }
        Frame frame;
        frame.kind = FrameKind::ValueMember;
        frame.targetObjectIndex = objectFrame.objectIndex;
        frame.source = node.Source();
        Base::Result<void> appended = frames_.PushBack(frame);
        if (!appended) return appended.GetStatus();
        return {};
    }

    if (node.IsFromAttribute() &&
        node.Name().LocalName() == DirectiveName) {
        return StartDirective(
            node,
            DirectiveKind::Name,
            objectFrame.objectIndex);
    }

    if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
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
        if (IsXamlDirective(node.Name(), DirectiveClass)) {
            return StartDirective(
                node,
                DirectiveKind::Class,
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

    ResolvedMember member;
    MemberWritePolicy memberPolicy;
    bool hasCompiledPolicy = false;
    bool memberValueTypeIsObject = false;
    bool memberValueTypeIsValueType = false;
    if (node.HasCompiledMemberBinding()) {
        member = ResolveCompiledMember(
            node.CompiledMember());
        memberPolicy = ResolveCompiledMemberPolicy(
            node.CompiledMember());
        hasCompiledPolicy = true;
        memberValueTypeIsObject =
            node.CompiledMember().ValueTypeIsObject();
        memberValueTypeIsValueType =
            node.CompiledMember().ValueTypeIsValueType();
        if (!IsCompiledMemberCompatible(
                *schema_,
                created_[objectFrame.objectIndex].type,
                member)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidAttachedMember.Data()),
                XamlObjectWriterDiagnosticCodes::
                    InvalidAttachedMember,
                MessageInvalidAttachedMember,
                node.Source());
        }
    } else {
        Base::Result<ResolvedMember> memberResult =
            node.CompiledMemberId() != Meta::InvalidMemberId
            ? schema_->ResolveMember(
                  created_[objectFrame.objectIndex].type,
                  node.CompiledMemberId())
            : schema_->ResolveMember(
                  created_[objectFrame.objectIndex].type,
                  node.Name(),
                  MemberSyntax::Attribute);
        if (!memberResult) {
            const bool notFound =
                memberResult.GetStatus().code ==
                    Base::ErrorCode::NotFound;
            return Failure(
                memberResult.GetStatus(),
                notFound
                    ? XamlObjectWriterDiagnosticCodes::UnknownMember
                    : XamlObjectWriterDiagnosticCodes::
                        InvalidAttachedMember,
                notFound
                    ? MessageUnknownMember
                    : MessageInvalidAttachedMember,
                node.Source());
        }
        member = memberResult.Value();
        memberPolicy =
            schema_->ResolveMemberWritePolicy(member);
        if (const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    member.valueType)) {
            memberValueTypeIsObject =
                valueType->Kind() ==
                    Meta::MetadataTypeKind::Object;
            memberValueTypeIsValueType =
                HasTypeFlag(
                    valueType->Flags(),
                    Meta::TypeFlags::ValueType);
        }
    }

    const bool eventAttribute =
        member.kind == Meta::MemberKind::Event &&
        node.IsFromAttribute();
    if (!eventAttribute &&
        (member.kind != Meta::MemberKind::Property ||
         !memberPolicy.writable)) {
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
    frame.memberPolicy = memberPolicy;
    frame.hasMemberPolicy =
        hasCompiledPolicy;
    frame.memberValueTypeIsObject =
        memberValueTypeIsObject;
    frame.memberValueTypeIsValueType =
        memberValueTypeIsValueType;
    frame.source = node.Source();
    frame.propertyElement = false;
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::StartDirective(
    const Node& node,
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
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> ObjectBuilder::EndMember(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueMember) {
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
    if (frame.valuesWritten == 0U && !frame.deferredStaticResource) {
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

Base::Result<void> ObjectBuilder::StartPropertyElement(
    const Node& node,
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
    ResolvedMember member;
    MemberWritePolicy memberPolicy;
    bool hasCompiledPolicy = false;
    bool memberValueTypeIsObject = false;
    bool memberValueTypeIsValueType = false;
    if (node.HasCompiledMemberBinding()) {
        member = ResolveCompiledMember(
            node.CompiledMember());
        memberPolicy = ResolveCompiledMemberPolicy(
            node.CompiledMember());
        hasCompiledPolicy = true;
        memberValueTypeIsObject =
            node.CompiledMember().ValueTypeIsObject();
        memberValueTypeIsValueType =
            node.CompiledMember().ValueTypeIsValueType();
        if (!IsCompiledMemberCompatible(
                *schema_,
                created_[targetObjectIndex].type,
                member)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidAttachedMember.Data()),
                XamlObjectWriterDiagnosticCodes::
                    InvalidAttachedMember,
                MessageInvalidAttachedMember,
                node.Source());
        }
    } else {
        Base::Result<ResolvedMember> memberResult =
            node.CompiledMemberId() != Meta::InvalidMemberId
            ? schema_->ResolveMember(
                  created_[targetObjectIndex].type,
                  node.CompiledMemberId())
            : schema_->ResolveMember(
                  created_[targetObjectIndex].type,
                  node.Name(),
                  MemberSyntax::PropertyElement);
        if (!memberResult) {
            const bool notFound =
                memberResult.GetStatus().code ==
                    Base::ErrorCode::NotFound;
            return Failure(
                memberResult.GetStatus(),
                notFound
                    ? XamlObjectWriterDiagnosticCodes::UnknownMember
                    : XamlObjectWriterDiagnosticCodes::
                        InvalidAttachedMember,
                notFound
                    ? MessageUnknownMember
                    : MessageInvalidAttachedMember,
                node.Source());
        }
        member = memberResult.Value();
        memberPolicy =
            schema_->ResolveMemberWritePolicy(member);
        if (const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    member.valueType)) {
            memberValueTypeIsObject =
                valueType->Kind() == Meta::MetadataTypeKind::Object;
            memberValueTypeIsValueType =
                HasTypeFlag(
                    valueType->Flags(),
                    Meta::TypeFlags::ValueType);
        }
    }

    const bool resourceEntries =
        created_[targetObjectIndex].type ==
            Aero::ResourceDictionary::StaticTypeId() &&
        created_[targetObjectIndex].hasContentMember &&
        created_[targetObjectIndex].contentMember.id == member.id;
    if (member.kind != Meta::MemberKind::Property ||
        (!memberPolicy.writable && !resourceEntries)) {
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
    frame.memberPolicy = memberPolicy;
    frame.hasMemberPolicy =
        hasCompiledPolicy;
    frame.memberValueTypeIsObject = memberValueTypeIsObject;
    frame.memberValueTypeIsValueType = memberValueTypeIsValueType;
    frame.source = node.Source();
    frame.propertyElement = true;
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::CompleteObject(
    const Node& node) noexcept {
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

    if (record.type ==
        StaticResourceObject::StaticTypeId()) {
        const auto& extension =
            static_cast<const StaticResourceObject&>(
                *record.object);
        if (extension.ResourceKey().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "StaticResource ResourceKey is empty"),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                MessageStaticResourceNotFound,
                node.Source());
        }
        Base::Result<Aero::ResourceValue> resource =
            LookupResource(extension.ResourceKey());
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                frames_.PopBack();
                PopNamespaceBindings(frame.namespaceBindingStart);
                if (!frames_.Empty()) {
                    Frame& parent = frames_.Back();
                    parent.deferredStaticResource = true;
                    DeferredStaticResourceRecord deferred;
                    if (parent.kind == FrameKind::Member) {
                        deferred.targetObjectIndex =
                            parent.targetObjectIndex;
                        deferred.member = parent.member;
                        deferred.policy = parent.hasMemberPolicy
                            ? parent.memberPolicy
                            : schema_->ResolveMemberWritePolicy(
                                parent.member);
                        deferred.hasPolicy = true;
                    } else if (parent.kind == FrameKind::Object &&
                               parent.objectIndex < created_.Size()) {
                        const CreatedObjectRecord& contentOwner =
                            created_[parent.objectIndex];
                        if (!contentOwner.hasContentMember) {
                            return Base::Status::Failure(
                                Base::ErrorCode::NotFound,
                                MessageMissingContentProperty.Data());
                        }
                        deferred.targetObjectIndex =
                            parent.objectIndex;
                        deferred.member =
                            contentOwner.contentMember;
                        deferred.policy =
                            contentOwner.contentPolicy;
                        deferred.hasPolicy = true;
                    } else {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "StaticResource parent frame is invalid");
                    }
                    if (deferred.targetObjectIndex < created_.Size()) {
                        created_[deferred.targetObjectIndex]
                            .deferredStaticResource = true;
                    }
                    deferred.source = node.Source();
                    Base::Result<void> key = deferred.key.Assign(
                        extension.ResourceKey());
                    if (!key) return key.GetStatus();
                    Base::Result<void> stored =
                        deferredStaticResources_.PushBack(
                            std::move(deferred));
                    if (!stored) return stored.GetStatus();
                }
                hasDeferredStaticResources_ = true;
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(extension.ResourceKey());
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        frames_.PopBack();
        PopNamespaceBindings(frame.namespaceBindingStart);
        return WriteValueToParent(
            std::move(resource).Value(), node.Source());
    }

    if (!record.name.Empty() && !record.nameRegistered) {
        Base::Result<void> nameResult = RegisterObjectName(
            objectIndex,
            node.Source());
        if (!nameResult) {
            return nameResult.GetStatus();
        }
    }

    // BasedOn="{StaticResource {x:Type T}}" is queued when the type key is
    // not yet visible (merged Source dictionaries commit after EndObject).
    // Sealing now would reject the deferred BasedOn write.
    if (!(record.deferredStaticResource &&
          record.type == Aero::Style::StaticTypeId())) {
    Base::Result<void> endResult = schema_->EndInit(
        record.type,
        *record.object,
        BuildExtensionServices(
            objectIndex,
            {},
            node.Source()));
    if (!endResult) {
        return Failure(
            endResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }
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

Base::Result<void> ObjectBuilder::CompleteValueObject(
    const Node& node) noexcept {
    if (frames_.Empty() ||
        frames_.Back().kind != FrameKind::ValueObject) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    const Frame frame = frames_.Back();
    if (frame.objectIndex >= created_.Size() ||
        created_[frame.objectIndex].value.IsUnset()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMissingMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::MissingMemberValue,
            MessageMissingMemberValue,
            node.Source());
    }
    frames_.PopBack();
    PopNamespaceBindings(frame.namespaceBindingStart);
    Base::Result<bool> resource = RegisterObjectResource(
        frame.objectIndex, node.Source());
    if (!resource) return resource.GetStatus();
    if (resource.Value()) return {};
    return WriteValueToParent(
        std::move(created_[frame.objectIndex].value),
        node.Source());
}

Base::Result<void> ObjectBuilder::CompleteNullObject(
    const Node& node) noexcept {
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


Base::Result<void> ObjectBuilder::ActivatePendingNamespaces(
    std::uint32_t& bindingStart) noexcept {
    bindingStart = namespaceBindings_.Size();
    for (PendingNamespaceRecord& pending : pendingNamespaces_) {
        NamespaceBindingRecord binding;
        Base::Result<void> prefixResult = binding.prefix.AssignUnchecked(
            pending.prefix.View());
        if (!prefixResult) {
            return prefixResult.GetStatus();
        }
        Base::Result<void> uriResult = binding.uri.AssignUnchecked(
            pending.uri.View());
        if (!uriResult) {
            return uriResult.GetStatus();
        }
        Base::Result<void> appendResult =
            namespaceBindings_.PushBack(std::move(binding));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
    }
    pendingNamespaces_.Clear();
    return {};
}

void ObjectBuilder::PopNamespaceBindings(
    std::uint32_t bindingStart) noexcept {
    if (bindingStart == InvalidIndex) {
        return;
    }
    while (namespaceBindings_.Size() > bindingStart) {
        namespaceBindings_.PopBack();
    }
}

Base::Result<Base::StringView> ObjectBuilder::LookupNamespace(
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

bool ObjectBuilder::IsXamlDirective(
    const QualifiedName& name,
    Base::StringView localName) const noexcept {
    return name.NamespaceUri() == LanguageNamespaceUri() &&
        name.LocalName() == localName;
}

bool ObjectBuilder::IsXamlNullObject(
    const QualifiedName& name) const noexcept {
    return IsXamlDirective(name, DirectiveNull);
}

bool ObjectBuilder::HasPropertyElementSyntax(
    const QualifiedName& name) const noexcept {
    for (char character : name.LocalName()) {
        if (character == '.') {
            return true;
        }
    }
    return false;
}

bool ObjectBuilder::IsWhitespaceOnly(
    Base::StringView value) const noexcept {
    for (char character : value) {
        if (!IsAsciiWhitespace(character)) {
            return false;
        }
    }
    return true;
}

void ObjectBuilder::CommitDocumentScopes() noexcept {
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

void ObjectBuilder::AbortTransaction() noexcept {
    root_.Reset();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
    deferredContent_.ReleaseAll();
    for (std::uint32_t index = extensionEffects_.Size();
         index > 0U; --index) {
        extensionEffects_[index - 1U].Rollback();
    }
    extensionEffects_.Clear();
    for (std::uint32_t index = created_.Size(); index > 0U; --index) {
        CreatedObjectRecord& record = created_[index - 1U];
        if (record.beginCalled && record.object) {
            schema_->AbortInit(record.type, *record.object);
        }
    }
    ClearTransaction();
}

void ObjectBuilder::ClearTransaction() noexcept {
    deferredContent_.ReleaseAll();
    frames_.Clear();
    assignments_.Clear();
    deferredStaticResources_.Clear();
    extensionEffects_.Clear();
    nameScopes_.Clear();
    resourceScopes_.Clear();
    serviceResourceChain_.Clear();
    namespaceBindings_.Clear();
    pendingNamespaces_.Clear();
    created_.Clear();
    root_.Reset();
    rootObjectIndex_ = InvalidIndex;
    documentNameScopeIndex_ = InvalidIndex;
    documentResourceScopeIndex_ = InvalidIndex;
    ended_ = false;
    hasDeferredStaticResources_ = false;
}

Base::Status ObjectBuilder::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode diagnostic,
    Base::StringView message,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(
            diagnostic,
            ::Aero::Diagnostics::DiagnosticSeverity::Error,
            message,
            source,
            ::Aero::Diagnostics::InvalidDiagnosticObjectId,
            Meta::InvalidMemberId);
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

ObjectWriter::ObjectWriter(
    ::Aero::Markup::Schema& schema,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics) {}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    NodeReader& reader) noexcept {
    ObjectBuilder state(*this);
    return state.Load(reader);
}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    const CompiledDocument& document) noexcept {
    ObjectBuilder state(*this);
    return state.Load(document);
}

} // namespace Aero::Markup

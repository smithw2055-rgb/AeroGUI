#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

#include "gui/markup/MarkupCommon.hpp"

// Modular markup extensions
#include "gui/markup/BindingExtension.inl"
#include "gui/markup/DynamicResourceExtension.inl"
#include "gui/markup/StaticResourceExtension.inl"
#include "gui/markup/LocExtension.inl"
#include "gui/markup/TemplateBindingExtension.inl"
#include "gui/markup/TypeExtension.inl"
#include "gui/markup/StaticExtension.inl"

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


// ===== ObjectBuilder =====






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
constexpr Base::StringView DirectiveClass("Class");
constexpr Base::StringView DirectiveNull("Null");
constexpr Base::StringView NullMarkup("x:Null");
constexpr Base::StringView StaticResourceMarkup("StaticResource");

class XamlEventConnection final : public Base::Object {
public:
    XamlEventConnection(
        Base::WeakRef<Base::Object> target,
        Meta::EventHandlerThunk thunk) noexcept
        : target_(std::move(target)),
          thunk_(thunk) {}

    void Invoke(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept {
        Base::Ref<Base::Object> target = target_.Lock();
        if (target && thunk_ != nullptr) {
            thunk_(target.Get(), sender, args);
        }
    }

private:
    Base::WeakRef<Base::Object> target_;
    Meta::EventHandlerThunk thunk_ = nullptr;
};

struct XamlEventInvoker {
    Base::Ref<XamlEventConnection> connection;

    void operator()(
        Base::Object* sender,
        RoutedEventArgs& args) const noexcept {
        if (connection) connection->Invoke(sender, args);
    }

    bool operator==(const XamlEventInvoker& other) const noexcept {
        return connection.Get() == other.connection.Get();
    }
};

Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
}

Base::Status SessionConsumedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML load session is single use");
}

Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept {
    Base::String message;
    Base::Result<void> appended = message.Assign(
        "StaticResource key '");
    if (appended) appended = message.Append(key);
    if (appended) appended = message.Append(
        "' is not available; forward references are not supported");
    return appended
        ? Base::Result<Base::String>(std::move(message))
        : Base::Result<Base::String>(appended.GetStatus());
}

bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept;
MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept;

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

Base::Result<void> ObjectBuilder::ResolveDeferredStaticResources() noexcept {
    for (DeferredStaticResourceRecord& deferred :
         deferredStaticResources_) {
        Base::Result<Aero::ResourceValue> resource =
            LookupResource(deferred.key.View());
        if (!resource) {
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(
                    deferred.key.View());
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                deferred.source);
        }
        Base::Result<void> written = WriteValue(
            deferred.targetObjectIndex,
            deferred.member,
            std::move(resource).Value(),
            deferred.source,
            deferred.hasPolicy
                ? &deferred.policy
                : nullptr);
        if (!written) return written.GetStatus();
    }
    deferredStaticResources_.Clear();
    hasDeferredStaticResources_ = false;
    return {};
}

Base::Result<void> ObjectBuilder::FinalizeDeferredStyles() noexcept {
    for (std::uint32_t index = 0U; index < created_.Size(); ++index) {
        CreatedObjectRecord& record = created_[index];
        if (!record.endCalled || !record.object) {
            continue;
        }
        if (record.type == Aero::Style::StaticTypeId()) {
            auto& style = static_cast<Aero::Style&>(*record.object);
            if (style.GetIsSealed()) {
                continue;
            }
            Base::Result<void> endResult = schema_->EndInit(
                record.type,
                *record.object,
                BuildExtensionServices(index, {}, {}));
            if (!endResult) {
                return endResult.GetStatus();
            }
        }
    }
    return {};
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

namespace {

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept;
MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept;

} // namespace

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

namespace {

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept {
    ResolvedMember member;
    member.id = binding.id;
    member.kind = binding.kind;
    member.ownerType = binding.ownerType;
    member.valueType = binding.valueType;
    member.propertyFlags = binding.propertyFlags;
    member.eventFlags = binding.eventFlags;
    member.attached = binding.attached;
    return member;
}

MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept {
    MemberWritePolicy policy;
    policy.mode = binding.writeMode ==
            static_cast<std::uint8_t>(
                MemberWriteMode::Collection)
        ? MemberWriteMode::Collection
        : MemberWriteMode::SetOnce;
    if (binding.id ==
        VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        policy.mode = MemberWriteMode::Collection;
    }
    policy.acceptsAnyValue =
        binding.acceptsAnyValue;
    policy.writable = binding.writable;
    return policy;
}

bool IsCompiledMemberCompatible(
    const Schema& schema,
    Meta::TypeId targetType,
    const ResolvedMember& member) noexcept {
    return member.attached ||
        schema.Types().IsDerivedFrom(
            targetType, member.ownerType);
}

} // namespace

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

Base::Result<void> ObjectBuilder::WriteText(
    const Node& node) noexcept {
    if (!node.HasCompiledValue() &&
        !node.IsFromAttribute() &&
        IsWhitespaceOnly(node.Value())) {
        // WPF TextBlock/Span mixed content collapses XML whitespace between
        // inlines to a single space so sibling Runs do not mash together
        // ("Release, Press or Hover" not "Release,PressorHover").
        if (frames_.Empty()) {
            return {};
        }
        Frame& spaceFrame = frames_.Back();
        std::uint32_t objectIndex = InvalidIndex;
        if (spaceFrame.kind == FrameKind::Member ||
            spaceFrame.kind == FrameKind::ValueMember) {
            objectIndex = spaceFrame.targetObjectIndex;
        } else if (spaceFrame.kind == FrameKind::Object ||
                   spaceFrame.kind == FrameKind::ValueObject) {
            objectIndex = spaceFrame.objectIndex;
        }
        if (objectIndex >= created_.Size() ||
            !created_[objectIndex].object) {
            return {};
        }
        const Meta::TypeId hostType = created_[objectIndex].type;
        const bool inlineHost =
            schema_->Types().IsDerivedFrom(
                hostType, Controls::TextBlock::StaticTypeId()) ||
            schema_->Types().IsDerivedFrom(
                hostType, Documents::Span::StaticTypeId());
        if (!inlineHost) {
            return {};
        }
        std::uint32_t inlineCount = 0U;
        Base::Object& host = *created_[objectIndex].object;
        if (schema_->Types().IsDerivedFrom(
                hostType, Controls::TextBlock::StaticTypeId())) {
            inlineCount = static_cast<Controls::TextBlock&>(host)
                .GetInlineCount();
        } else {
            inlineCount = static_cast<Documents::Span&>(host)
                .GetInlines().GetCount();
        }
        if (inlineCount == 0U) {
            return {};
        }
        Base::Result<Meta::Value> space =
            Meta::Value::TryFromString(
                Meta::TypeOf<Base::String>(),
                Base::StringView(" "));
        if (!space) {
            return space.GetStatus();
        }
        if (spaceFrame.kind == FrameKind::Member) {
            return WriteValueToMember(
                spaceFrame, std::move(space).Value(), node.Source());
        }
        if (spaceFrame.kind == FrameKind::Object &&
            created_[objectIndex].hasContentMember) {
            return WriteValue(
                objectIndex,
                created_[objectIndex].contentMember,
                std::move(space).Value(),
                node.Source(),
                &created_[objectIndex].contentPolicy);
        }
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
    if (node.HasCompiledValue()) {
        Meta::Value value = node.CompiledValue();
        if (frame.kind == FrameKind::ValueMember ||
            frame.kind == FrameKind::ValueObject) {
            const std::uint32_t objectIndex =
                frame.kind == FrameKind::ValueMember
                    ? frame.targetObjectIndex
                    : frame.objectIndex;
            if (objectIndex >= created_.Size() ||
                !created_[objectIndex].valueElement ||
                !created_[objectIndex].value.IsUnset() ||
                frame.valuesWritten != 0U) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        MessageDuplicateMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::
                        DuplicateMemberValue,
                    MessageDuplicateMemberValue,
                    node.Source());
            }
            created_[objectIndex].value = std::move(value);
            ++frame.valuesWritten;
            return {};
        }
        if (frame.kind == FrameKind::Member) {
            return WriteValueToMember(
                frame, std::move(value), node.Source());
        }
        if (frame.kind == FrameKind::Object &&
            frame.objectIndex < created_.Size()) {
            const CreatedObjectRecord& object =
                created_[frame.objectIndex];
            if (!object.hasContentMember) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        MessageMissingContentProperty.Data()),
                    XamlObjectWriterDiagnosticCodes::
                        MissingContentProperty,
                    MessageMissingContentProperty,
                    node.Source());
            }
            return WriteValue(
                frame.objectIndex,
                object.contentMember,
                std::move(value),
                node.Source(),
                &object.contentPolicy);
        }
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frame.kind == FrameKind::Directive) {
        return WriteDirectiveText(frame, node);
    }

    if (frame.kind == FrameKind::ValueMember ||
        frame.kind == FrameKind::ValueObject) {
        const std::uint32_t objectIndex =
            frame.kind == FrameKind::ValueMember
                ? frame.targetObjectIndex
                : frame.objectIndex;
        if (objectIndex >= created_.Size() ||
            !created_[objectIndex].valueElement ||
            !created_[objectIndex].value.IsUnset() ||
            frame.valuesWritten != 0U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageDuplicateMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
                MessageDuplicateMemberValue,
                node.Source());
        }
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(), extensionName, argument);
        if (markup != MarkupValueKind::Literal &&
            markup != MarkupValueKind::EscapedLiteral) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        ResolvedMember valueMember;
        valueMember.valueType = created_[objectIndex].type;
        const ExtensionServices services = BuildExtensionServices(
            objectIndex,
            valueMember,
            node.Source());
        Base::Result<Meta::Value> converted = schema_->ConvertText(
            created_[objectIndex].type,
            markup == MarkupValueKind::EscapedLiteral
                ? argument : node.Value(),
            &services);
        if (!converted) {
            return Failure(
                converted.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        created_[objectIndex].value = std::move(converted).Value();
        ++frame.valuesWritten;
        return {};
    }

    if (frame.kind == FrameKind::Member) {
        if (frame.member.kind == Meta::MemberKind::Event) {
            if (frame.valuesWritten != 0U ||
                node.HasCompiledValue()) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        MessageDuplicateMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
                    MessageDuplicateMemberValue,
                    node.Source());
            }
            Base::Result<void> connected = ConnectEvent(
                frame, TrimAscii(node.Value()), node.Source());
            if (!connected) return connected.GetStatus();
            ++frame.valuesWritten;
            return {};
        }
        const MemberWritePolicy policy =
            frame.hasMemberPolicy
            ? frame.memberPolicy
            : schema_->ResolveMemberWritePolicy(
                  frame.member);
        const bool acceptsAnyValue =
            policy.acceptsAnyValue;
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
            if (frame.targetObjectIndex < created_.Size() &&
                frame.member.id ==
                    Controls::Primitives::ToggleButton::
                        IsCheckedProperty.Handle().value &&
                schema_->Types().IsDerivedFrom(
                    created_[frame.targetObjectIndex].type,
                    Controls::Primitives::ToggleButton::
                        StaticTypeId())) {
                Base::Result<Meta::Value> nullable =
                    Meta::ValueCodec<Nullable<bool>>::Encode(
                        Nullable<bool>{});
                if (!nullable) return nullable.GetStatus();
                Base::Result<void> written = WriteValueToMember(
                    frame, std::move(nullable).Value(), node.Source());
                if (!written) return written.GetStatus();
                return {};
            }
            if (frame.member.valueType == Meta::TypeOf<Base::String>()) {
                Base::Result<Meta::Value> empty =
                    Meta::Value::TryFromString(
                        Meta::TypeOf<Base::String>(), {});
                if (!empty) return empty.GetStatus();
                return WriteValueToMember(
                    frame, std::move(empty).Value(), node.Source());
            }
            if (frame.memberValueTypeIsValueType &&
                !acceptsAnyValue) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
            Meta::Value value = Meta::Value::NullObject(frame.member.valueType);
            return WriteValueToMember(frame, std::move(value), node.Source());
        }
        if (markup == MarkupValueKind::StaticResource) {
            Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
            if (!resource) {
                if (loadContext_ != nullptr &&
                    loadContext_->deferUnresolvedStaticResources) {
                    frame.deferredStaticResource = true;
                    hasDeferredStaticResources_ = true;
                    if (frame.targetObjectIndex < created_.Size()) {
                        created_[frame.targetObjectIndex]
                            .deferredStaticResource = true;
                    }
                    DeferredStaticResourceRecord deferred;
                    deferred.targetObjectIndex =
                        frame.targetObjectIndex;
                    deferred.member = frame.member;
                    deferred.policy = policy;
                    deferred.hasPolicy = true;
                    deferred.source = node.Source();
                    Base::Result<void> key = deferred.key.Assign(
                        argument);
                    if (!key) return key.GetStatus();
                    Base::Result<void> stored =
                        deferredStaticResources_.PushBack(
                            std::move(deferred));
                    if (!stored) return stored.GetStatus();
                    return {};
                }
                Base::Result<Base::String> message =
                    StaticResourceNotFoundMessage(argument);
                if (!message) return message.GetStatus();
                return Failure(
                    resource.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                    message.Value().View(),
                    node.Source());
            }
            return WriteValueToMember(
                frame, std::move(resource).Value(), node.Source());
        }

        if (markup == MarkupValueKind::Extension) {
            Base::Result<ProvidedValue> value =
                EvaluateMarkupExtension(
                    frame.targetObjectIndex,
                    frame.member,
                    extensionName,
                    argument,
                    node.Source());
            if (!value) return value.GetStatus();
            return WriteProvidedValueToMember(
                frame,
                std::move(value).Value(),
                node.Source());
        }

        const ExtensionServices services = BuildExtensionServices(
            frame.targetObjectIndex,
            frame.member,
            node.Source());
        Base::Result<Meta::Value> convertResult = schema_->ConvertText(
            frame.member.valueType,
            markup == MarkupValueKind::EscapedLiteral
                ? argument
                : node.Value(),
            &services);
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

    const CreatedObjectRecord& contentOwner =
        created_[frame.objectIndex];
    if (!contentOwner.hasContentMember) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }
    const ResolvedMember& contentMember =
        contentOwner.contentMember;

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
        if (contentOwner.contentValueTypeIsValueType) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                node.Source());
        }
        Meta::Value value = Meta::Value::NullObject(
            contentMember.valueType);
        return WriteValue(
            frame.objectIndex,
            contentMember,
            std::move(value),
            node.Source(),
            &contentOwner.contentPolicy);
    }
    if (markup == MarkupValueKind::StaticResource) {
        Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                frame.deferredStaticResource = true;
                hasDeferredStaticResources_ = true;
                if (frame.objectIndex < created_.Size()) {
                    created_[frame.objectIndex].deferredStaticResource =
                        true;
                }
                DeferredStaticResourceRecord deferred;
                deferred.targetObjectIndex = frame.objectIndex;
                deferred.member = contentMember;
                deferred.policy = contentOwner.contentPolicy;
                deferred.hasPolicy = true;
                deferred.source = node.Source();
                Base::Result<void> key = deferred.key.Assign(
                    argument);
                if (!key) return key.GetStatus();
                Base::Result<void> stored =
                    deferredStaticResources_.PushBack(
                        std::move(deferred));
                if (!stored) return stored.GetStatus();
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(argument);
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        return WriteValue(
            frame.objectIndex,
            contentMember,
            std::move(resource).Value(),
            node.Source(),
            &contentOwner.contentPolicy);
    }

    if (markup == MarkupValueKind::Extension) {
        Base::Result<ProvidedValue> value =
            EvaluateMarkupExtension(
                frame.objectIndex,
                contentMember,
                extensionName,
                argument,
                node.Source());
        if (!value) return value.GetStatus();
        return WriteProvidedValue(
            frame.objectIndex,
            contentMember,
            std::move(value).Value(),
            node.Source(),
            &contentOwner.contentPolicy);
    }

    const ExtensionServices services = BuildExtensionServices(
        frame.objectIndex,
        contentMember,
        node.Source());
    const bool itemsControlScalarContent =
        schema_->Types().IsDerivedFrom(
            contentOwner.type,
            Controls::ItemsControl::StaticTypeId()) &&
        contentOwner.hasContentMember &&
        contentOwner.contentMember.id == contentMember.id;
    Base::Result<Meta::Value> convertResult = schema_->ConvertText(
        itemsControlScalarContent
            ? Meta::TypeOf<Base::String>()
            : contentMember.valueType,
        markup == MarkupValueKind::EscapedLiteral
            ? argument
            : node.Value(),
        &services);
    if (!convertResult) {
        return Failure(
            convertResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            node.Source());
    }
    return WriteValue(
        frame.objectIndex,
        contentMember,
        std::move(convertResult).Value(),
        node.Source(),
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectBuilder::WriteDirectiveText(
    Frame& frame,
    const Node& node) noexcept {
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
        const Meta::PropertyInfo* nameProperty =
            schema_->Metadata()->Types().FindProperty(
                object.type, Base::StringView("Name"), false);
        const bool validName =
            Aero::NameScope::IsValidName(node.Value());
        if (!validName && nameProperty == nullptr) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        if (validName) {
            Base::Result<void> assignResult = object.name.Assign(node.Value());
            if (!assignResult) {
                return assignResult.GetStatus();
            }
            Base::Result<void> registerResult = RegisterObjectName(
                frame.targetObjectIndex,
                node.Source());
            if (!registerResult) {
                return registerResult.GetStatus();
            }
        }
        // VisualState and related non-visual authoring objects expose an
        // ordinary Name property in addition to participating in x:Name
        // scopes. Types such as Scoreboard's Game also expose Name as a
        // display string that is not a runtime name (spaces are allowed).
        if (nameProperty != nullptr) {
            Base::Result<Meta::Value> nameValue =
                schema_->ConvertText(
                    nameProperty->ValueType(), node.Value(), nullptr);
            if (!nameValue) return nameValue.GetStatus();
            ResolvedMember nameMember;
            nameMember.id = nameProperty->Id();
            nameMember.kind = Meta::MemberKind::Property;
            nameMember.ownerType = nameProperty->OwnerType();
            nameMember.valueType = nameProperty->ValueType();
            nameMember.propertyFlags = nameProperty->Flags();
            Base::Result<void> assignedName = WriteValue(
                frame.targetObjectIndex, nameMember,
                std::move(nameValue).Value(), node.Source());
            if (!assignedName) return assignedName.GetStatus();
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
        Base::Result<void> assignResult = object.key.Assign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Class) {
        if (node.Value().Empty() ||
            frame.targetObjectIndex != rootObjectIndex_) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        const Base::StringView className = TrimAscii(node.Value());
        std::uint32_t separator = className.SizeBytes();
        for (std::uint32_t index = 0U;
             index < className.SizeBytes(); ++index) {
            if (className[index] == '.') separator = index;
        }
        if (separator == 0U ||
            separator + 1U >= className.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "x:Class must use Namespace.Type syntax"),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::String xamlNamespace;
        Base::Result<void> namespaceAssigned =
            xamlNamespace.Assign("clr-namespace:");
        if (namespaceAssigned) {
            namespaceAssigned = xamlNamespace.Append(
                className.Substr(0U, separator));
        }
        if (!namespaceAssigned) return namespaceAssigned.GetStatus();
        const Base::StringView localName = className.Substr(
            separator + 1U,
            className.SizeBytes() - separator - 1U);
        Base::Result<const Meta::TypeInfo*> classType =
            schema_->ResolveType(xamlNamespace.View(), localName);
        if (!classType) {
            if (loadContext_ != nullptr && loadContext_->existingRoot) {
                return Failure(
                    classType.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::UnknownType,
                    MessageUnknownType,
                    node.Source());
            }
            if (classType.GetStatus().code ==
                Base::ErrorCode::NotFound) {
                // A pure-XAML host may intentionally omit the code-behind
                // class. Keep the authored root type in that case while
                // activating a registered derived class when one exists.
                frame.valuesWritten = 1U;
                return {};
            }
            return classType.GetStatus();
        }
        if (!schema_->Types().IsDerivedFrom(
                classType.Value()->Id(), object.type)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "x:Class type does not derive from the authored root type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        if (loadContext_ != nullptr && loadContext_->existingRoot &&
            frame.targetObjectIndex == rootObjectIndex_ &&
            classType.Value()->Id() !=
                loadContext_->existingRoot->RuntimeType()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "x:Class does not match the existing root runtime type"),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                node.Source());
        }
        if (classType.Value()->Id() != object.type) {
            Base::Result<Base::Ref<Base::Object>> replacement =
                CreateObject(classType.Value()->Id());
            if (!replacement) {
                return Failure(
                    replacement.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::FactoryFailed,
                    MessageFactoryFailed,
                    node.Source());
            }
            Base::Result<void> initialized = schema_->BeginInit(
                classType.Value()->Id(), *replacement.Value());
            if (!initialized) return initialized.GetStatus();
            if (object.beginCalled && object.object) {
                schema_->AbortInit(object.type, *object.object);
            }
            object.object = std::move(replacement).Value();
            object.type = classType.Value()->Id();
            object.beginCalled = true;
            object.endCalled = false;
            object.hasContentMember = false;
            object.contentMember = {};
            object.contentPolicy = {};
            object.contentValueTypeIsObject = false;
            object.contentValueTypeIsValueType = false;
            Base::Result<ResolvedMember> content =
                schema_->ResolveContentMember(object.type);
            if (content) {
                object.contentMember = content.Value();
                object.contentPolicy =
                    schema_->ResolveMemberWritePolicy(
                        object.contentMember);
                object.hasContentMember = true;
                if (const Meta::TypeInfo* valueType =
                        schema_->Types().FindType(
                            object.contentMember.valueType)) {
                    object.contentValueTypeIsObject =
                        valueType->Kind() ==
                            Meta::MetadataTypeKind::Object;
                    object.contentValueTypeIsValueType =
                        valueType->Kind() !=
                            Meta::MetadataTypeKind::Object &&
                        HasTypeFlag(
                            valueType->Flags(),
                            Meta::TypeFlags::ValueType);
                }
            } else if (content.GetStatus().code !=
                       Base::ErrorCode::NotFound) {
                return content.GetStatus();
            }
            const std::uint32_t objectFrame =
                FindObjectFrameIndex(frame.targetObjectIndex);
            if (objectFrame != InvalidIndex &&
                frames_[objectFrame].resourceScopeIndex != InvalidIndex &&
                frames_[objectFrame].resourceScopeIndex <
                    resourceScopes_.Size()) {
                resourceScopes_[
                    frames_[objectFrame].resourceScopeIndex].external =
                        schema_->ResolveResourceScope(
                            object.type, *object.object);
            }
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

Base::Result<void> ObjectBuilder::WriteValueToParent(
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            source);
    }
    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
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
    Base::Result<ResolvedMember> content =
        schema_->ResolveContentMember(
            created_[parent.objectIndex].type);
    if (!content) {
        return Failure(
            content.GetStatus(),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }
    return WriteValue(
        parent.objectIndex,
        content.Value(),
        std::move(value),
        source);
}

Base::Result<void> ObjectBuilder::WriteObjectToParent(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
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
        Meta::Value value = Meta::Value::FromObject(
            created_[objectIndex].type,
            created_[objectIndex].object);
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

Base::Result<void> ObjectBuilder::WriteObjectToContent(
    std::uint32_t parentObjectIndex,
    std::uint32_t childObjectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (parentObjectIndex >= created_.Size() ||
        childObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const CreatedObjectRecord& contentOwner =
        created_[parentObjectIndex];
    if (!contentOwner.hasContentMember) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMissingContentProperty.Data()),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }

    Meta::Value value = Meta::Value::FromObject(
        created_[childObjectIndex].type,
        created_[childObjectIndex].object);
    return WriteValue(
        parentObjectIndex,
        contentOwner.contentMember,
        std::move(value),
        source,
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectBuilder::WriteNullToParent(
    ::Aero::Diagnostics::SourceSpan source) noexcept {
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
        if (parent.targetObjectIndex < created_.Size() &&
            parent.member.id ==
                Controls::Primitives::ToggleButton::
                    IsCheckedProperty.Handle().value &&
            schema_->Types().IsDerivedFrom(
                created_[parent.targetObjectIndex].type,
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            Base::Result<Meta::Value> nullable =
                Meta::ValueCodec<Nullable<bool>>::Encode(
                    Nullable<bool>{});
            if (!nullable) return nullable.GetStatus();
            Base::Result<void> written = WriteValueToMember(
                parent, std::move(nullable).Value(), source);
            if (!written) return written.GetStatus();
            return {};
        }
        const MemberWritePolicy policy =
            parent.hasMemberPolicy
            ? parent.memberPolicy
            : schema_->ResolveMemberWritePolicy(
                  parent.member);
        const bool acceptsAnyValue =
            policy.acceptsAnyValue;
        if (parent.member.valueType == Meta::TypeOf<Base::String>()) {
            Base::Result<Meta::Value> empty =
                Meta::Value::TryFromString(
                    Meta::TypeOf<Base::String>(), {});
            if (!empty) return empty.GetStatus();
            return WriteValueToMember(
                parent, std::move(empty).Value(), source);
        }
        if (parent.memberValueTypeIsValueType &&
            !acceptsAnyValue) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                source);
        }
        Meta::Value value = Meta::Value::NullObject(parent.member.valueType);
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

    const CreatedObjectRecord& contentOwner =
        created_[parent.objectIndex];
    if (!contentOwner.hasContentMember ||
        contentOwner.contentValueTypeIsValueType) {
        return Failure(
            Base::Status::Failure(
                contentOwner.hasContentMember
                    ? Base::ErrorCode::ValidationFailed
                    : Base::ErrorCode::NotFound,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Meta::Value value = Meta::Value::NullObject(
        contentOwner.contentMember.valueType);
    return WriteValue(
        parent.objectIndex,
        contentOwner.contentMember,
        std::move(value),
        source,
        &contentOwner.contentPolicy);
}

Base::Result<void> ObjectBuilder::WriteValueToMember(
    Frame& memberFrame,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(value),
        source,
        memberFrame.hasMemberPolicy
            ? &memberFrame.memberPolicy
            : nullptr);
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

Base::Result<void> ObjectBuilder::WriteProvidedValueToMember(
    Frame& memberFrame,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteProvidedValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(provided),
        source,
        memberFrame.hasMemberPolicy
            ? &memberFrame.memberPolicy
            : nullptr);
    if (!result) return result.GetStatus();
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

Base::Result<void> ObjectBuilder::WriteProvidedValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source,
    const MemberWritePolicy* compiledPolicy) noexcept {
    if (provided.kind == ProvidedValueKind::Value) {
        return WriteValue(
            targetObjectIndex,
            member,
            std::move(provided.value),
            source,
            compiledPolicy);
    }
    if (targetObjectIndex >= created_.Size()) {
        provided.Discard();
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const MemberWritePolicy policy =
        compiledPolicy != nullptr
        ? *compiledPolicy
        : schema_->ResolveMemberWritePolicy(member);
    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex, member.id);
    if (!policy.writable ||
        (assignment != nullptr && assignment->count != 0U &&
         policy.mode == MemberWriteMode::SetOnce)) {
        provided.Discard();
        return Failure(
            Base::Status::Failure(
                policy.writable
                    ? Base::ErrorCode::AlreadyExists
                    : Base::ErrorCode::Unsupported,
                policy.writable
                    ? MessageDuplicateMemberValue.Data()
                    : MessageUnsupportedMember.Data()),
            policy.writable
                ? XamlObjectWriterDiagnosticCodes::DuplicateMemberValue
                : XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            policy.writable
                ? MessageDuplicateMemberValue
                : MessageUnsupportedMember,
            source);
    }
    if (assignment == nullptr) {
        Base::Result<void> appended = assignments_.PushBack({
            targetObjectIndex, member.id, 0U});
        if (!appended) {
            provided.Discard();
            return appended.GetStatus();
        }
        assignment = &assignments_.Back();
    }

    CommittedEffect effect;
    if (loadContext_ != nullptr) {
        effect.lifetime = loadContext_->effectLifetime;
    }
    if (provided.kind == ProvidedValueKind::Expression) {
        if (provided.effectiveValues == nullptr ||
            !provided.expression.IsValid()) {
            provided.Discard();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup extension returned an invalid property expression");
        }
        Base::Result<::Aero::DependencyObject*> target =
            schema_->ResolvePropertyTarget(
                *created_[targetObjectIndex].object);
        if (!target) {
            provided.Discard();
            return target.GetStatus();
        }
        effect.effectiveValues = provided.effectiveValues;
        effect.targetOwner =
            Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(
                *target.Value());
        if (!effect.targetOwner) {
            provided.Discard();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup expression target is not reference-counted");
        }
        effect.target = effect.targetOwner.Get();
        effect.property = Meta::DependencyPropertyHandle{member.id};
        effect.pendingExpression = provided.expression;
        provided.expression = {};
    } else if (provided.kind == ProvidedValueKind::Handled) {
        effect.context = provided.rollbackContext;
        effect.token = provided.rollbackToken;
        effect.rollback = provided.rollback;
        effect.committed = true;
        provided.rollbackContext = nullptr;
        provided.rollback = nullptr;
    } else if (provided.kind == ProvidedValueKind::Deferred) {
        effect.context = provided.rollbackContext;
        effect.prepare = provided.prepare;
        effect.commit = provided.commit;
        effect.rollback = provided.rollback;
        effect.cleanup = provided.cleanup;
        effect.bind = provided.bind;
        provided.rollbackContext = nullptr;
        provided.prepare = nullptr;
        provided.commit = nullptr;
        provided.rollback = nullptr;
        provided.cleanup = nullptr;
        provided.bind = nullptr;
    } else {
        provided.Discard();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Markup extension returned an unknown value kind");
    }

    const bool immediate = loadContext_ == nullptr ||
        loadContext_->effectCommitMode == EffectCommitMode::Immediate;
    if (immediate && !effect.committed) {
        Base::Result<void> committed = effect.Commit();
        if (!committed) {
            effect.Rollback();
            return committed.GetStatus();
        }
    }
    Base::Result<void> effectStored =
        extensionEffects_.PushBack(std::move(effect));
    if (!effectStored) {
        effect.Rollback();
        return effectStored.GetStatus();
    }
    if (assignment->count == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            MessageDuplicateMemberValue.Data());
    }
    ++assignment->count;
    return {};
}

Base::Result<void> ObjectBuilder::WriteValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source,
    const MemberWritePolicy* compiledPolicy) noexcept {
    if (targetObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        if (MarkupExtension* extension =
                ::Aero::TryCast<MarkupExtension>(value.AsObject().Get())) {
            Base::Result<Meta::Value> provided = extension->ProvideValue();
            if (!provided) {
                return Failure(
                    provided.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
                    MessageMarkupExtensionFailed,
                    source);
            }
            value = std::move(provided).Value();
        }
    }
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::MultiBinding::StaticTypeId()) {
        const ExtensionServices services =
            BuildExtensionServices(
                targetObjectIndex,
                member,
                source);
        Base::Result<ProvidedValue> provided =
            CreateMultiBindingValue(
                static_cast<Data::MultiBinding&>(
                    *value.AsObject()),
                services);
        if (!provided) {
            return Failure(
                provided.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                source);
        }
        return WriteProvidedValue(
            targetObjectIndex,
            member,
            std::move(provided).Value(),
            source,
            compiledPolicy);
    }


    const MemberWritePolicy policy =
        compiledPolicy != nullptr
        ? *compiledPolicy
        : schema_->ResolveMemberWritePolicy(member);
    if (!policy.writable) {
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
        policy.mode == MemberWriteMode::SetOnce) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }

    if (assignment == nullptr) {
        Base::Result<void> appendResult = assignments_.PushBack({
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

    CreatedObjectRecord& targetRecord =
        created_[targetObjectIndex];
    if (value.Kind() != Meta::ValueKind::Object &&
        targetRecord.hasContentMember &&
        targetRecord.contentMember.id == member.id &&
        schema_->Types().IsDerivedFrom(
            targetRecord.type,
            Controls::ItemsControl::StaticTypeId())) {
        Base::Result<Base::Ref<
            Controls::BoxedItemValue>> boxed =
                Base::MakeRef<Controls::BoxedItemValue>(
                    value);
        if (!boxed) return boxed.GetStatus();
        value = Meta::Value::FromObject(
            member.valueType,
            Base::Ref<Base::Object>(
                std::move(boxed).Value()));
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    if ((member.valueType == Meta::TypeOf<Aero::Length>() ||
         member.valueType == Meta::TypeOf<::Aero::GridLength>()) &&
        value.Type() != member.valueType &&
        (value.Kind() == Meta::ValueKind::SignedInteger ||
         value.Kind() == Meta::ValueKind::UnsignedInteger ||
         value.Kind() == Meta::ValueKind::Double)) {
        Base::Result<Meta::Value> converted =
            ConvertConstantBindingValue(
                value, member.valueType);
        if (!converted) {
            return Failure(
                converted.GetStatus(),
                XamlObjectWriterDiagnosticCodes::TypeMismatch,
                MessageTypeMismatch,
                source);
        }
        value = std::move(converted).Value();
    }
    if (member.valueType ==
            Media::Brush::StaticTypeId() &&
        value.Type() ==
            Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                value);
        if (!color) {
            return Failure(
                color.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        Base::Result<
            Base::Ref<Media::Brush>>
            brush =
                Media::MakeSolidColorBrush(
                    color.Value());
        if (!brush) {
            return Failure(
                brush.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        value = Meta::Value::FromObject(
            Media::Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    Base::Result<Meta::ContentInfo> content =
        schema_->Metadata()->GetContentInfo(member.id);
    const bool hasWritableContent =
        content && content.Value().writable;
    const bool hasVisualContent =
        hasWritableContent && content.Value().IsVisual();
    const Meta::PropertyInfo* memberProperty =
        schema_->Types().FindProperty(member.id);
    const auto isUIElementValue =
        [this](const Meta::Value& candidate) noexcept {
            return candidate.Kind() == Meta::ValueKind::Object &&
                !candidate.IsNullObject() &&
                candidate.AsObject() &&
                schema_->Types().IsDerivedFrom(
                    candidate.AsObject()->RuntimeType(),
                    Aero::UIElement::StaticTypeId());
        };
    const bool hasVisualStructuralProperty =
        memberProperty != nullptr &&
        (static_cast<std::uint32_t>(
             memberProperty->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        schema_->Metadata()->
            CanWriteProperty(member.id);
    const bool isDependencyObjectValue =
        value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject() &&
        schema_->Types().IsDerivedFrom(
            value.AsObject()->RuntimeType(),
            Aero::DependencyObject::StaticTypeId());
    const bool stagesVisualContent =
        (hasVisualContent ||
         hasVisualStructuralProperty) &&
        isUIElementValue(value);
    const bool parentIsVisual =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Aero::Media::Visual::StaticTypeId());
    const bool parentIsFreezable =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Aero::Freezable::StaticTypeId());
    const bool parentIsTimeline =
        schema_->Types().IsDerivedFrom(
            created_[targetObjectIndex].type,
            Media::Animation::Timeline::StaticTypeId());
    const bool stagesDeferredObjectContent =
        services.deferredContentOwner != nullptr &&
        (hasWritableContent || hasVisualStructuralProperty) &&
        isDependencyObjectValue &&
        (services.deferredContentOwner ==
             created_[targetObjectIndex].object.Get() ||
         parentIsVisual ||
         (parentIsFreezable && !parentIsTimeline));
    Base::Result<void> setResult =
        (stagesVisualContent || stagesDeferredObjectContent)
        ? ObjectWriter::StageContent(
              *schema_,
              *created_[targetObjectIndex].object,
              value,
              services)
        : schema_->SetMember(
              *created_[targetObjectIndex].object,
              created_[targetObjectIndex].type,
              member,
              value);
    if (setResult &&
        hasVisualContent &&
        !stagesVisualContent &&
        schema_->Metadata()->CanReadProperty(member.id)) {
        Base::Result<Meta::Value> materialized =
            schema_->Metadata()->GetProperty(
                *created_[targetObjectIndex].object,
                member.id);
        if (materialized &&
            isUIElementValue(materialized.Value())) {
            setResult = ObjectWriter::StageContent(
                *schema_,
                *created_[targetObjectIndex].object,
                materialized.Value(),
                services);
        }
    }
    if (!setResult) {
        ::Aero::Diagnostics::DiagnosticCode code =
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

Base::Result<void> ObjectBuilder::RegisterObjectName(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
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
    Base::Result<void> localResult = scope.names.Register(
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

Base::Result<void> ObjectBuilder::ConnectEvent(
    Frame& memberFrame,
    Base::StringView handlerName,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (handlerName.Empty() ||
        memberFrame.targetObjectIndex >= created_.Size() ||
        rootObjectIndex_ >= created_.Size() ||
        memberFrame.member.kind != Meta::MemberKind::Event) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event attribute requires a handler name"),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            source);
    }

    CreatedObjectRecord& eventSource =
        created_[memberFrame.targetObjectIndex];
    CreatedObjectRecord& codeBehind =
        created_[rootObjectIndex_];
    if (!eventSource.object || !codeBehind.object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Meta::EventHandlerThunk thunk =
        schema_->Metadata()->Types().FindEventHandler(
            codeBehind.type,
            handlerName);
    if (thunk == nullptr) {
        // LoadComponentInto a real code-behind instance must resolve the
        // handler. A pure-XAML host (x:Class omitted or unregistered) stores
        // Click="OnFoo" the same way CommandBinding Executed stores a name:
        // do not abort document construction.
        if (loadContext_ != nullptr && loadContext_->existingRoot) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "XAML event handler is not registered on the x:Class type"),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                source);
        }
        return {};
    }

    Base::Result<Base::Ref<XamlEventConnection>> connection =
        Base::MakeRef<XamlEventConnection>(
            Base::WeakRef<Base::Object>(root_),
            thunk);
    if (!connection) return connection.GetStatus();
    Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(
        XamlEventInvoker{std::move(connection).Value()});
    const RoutedEventHandle routedEvent{memberFrame.member.id};

    Base::Result<void> connected;
    if (schema_->Types().IsDerivedFrom(
            eventSource.type, UIElement::StaticTypeId())) {
        connected = static_cast<UIElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else if (schema_->Types().IsDerivedFrom(
                   eventSource.type,
                   ContentElement::StaticTypeId())) {
        connected = static_cast<ContentElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else {
        connected = Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML event source does not support routed handlers");
    }
    return connected
        ? Base::Result<void>{}
        : Base::Result<void>(Failure(
              connected.GetStatus(),
              XamlObjectWriterDiagnosticCodes::UnsupportedMember,
              MessageUnsupportedMember,
              source));
}

Base::Result<bool> ObjectBuilder::RegisterObjectResource(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.resourceRegistered) {
        return true;
    }

    bool dictionaryContent = false;
    if (!frames_.Empty()) {
        const Frame& parent = frames_.Back();
        std::uint32_t parentObjectIndex = InvalidIndex;
        Meta::MemberId targetMember = Meta::InvalidMemberId;
        if (parent.kind == FrameKind::Object) {
            parentObjectIndex = parent.objectIndex;
        } else if (parent.kind == FrameKind::Member) {
            parentObjectIndex = parent.targetObjectIndex;
            targetMember = parent.member.id;
        }
        if (parentObjectIndex < created_.Size() &&
            created_[parentObjectIndex].type ==
                Aero::ResourceDictionary::StaticTypeId()) {
            const CreatedObjectRecord& dictionary =
                created_[parentObjectIndex];
            dictionaryContent =
                dictionary.hasContentMember &&
                (targetMember == Meta::InvalidMemberId ||
                 targetMember ==
                     dictionary.contentMember.id);
        } else if (
            parentObjectIndex < created_.Size() &&
            targetMember != Meta::InvalidMemberId &&
            object.type !=
                Aero::ResourceDictionary::
                    StaticTypeId() &&
            schema_->CreatesResourceScope(
                created_[parentObjectIndex].type)) {
            dictionaryContent =
                parent.kind == FrameKind::Member &&
                parent.member.valueType ==
                    Aero::ResourceDictionary::
                        StaticTypeId();
        }
    }

    const bool explicitKey = !object.key.Empty();
    if (!dictionaryContent) {
        if (!explicitKey) return false;
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            source);
    }

    Base::Result<Aero::ResourceKey> resourceKey =
        explicitKey
        ? Aero::ResourceKey::FromString(
              object.key.View())
        : (object.valueElement || !object.object
            ? Base::Result<Aero::ResourceKey>(
                  Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Unkeyed ResourceDictionary entry must be an object"))
            : schema_->ResolveImplicitResourceKey(
                  object.type,
                  *object.object));
    if (!resourceKey) {
        return Failure(
            resourceKey.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
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
    Meta::Value resourceValue = object.valueElement
        ? object.value
        : Meta::Value::FromObject(object.type, object.object);
    Base::Result<void> localResult = scope.resources.Add(
        resourceKey.Value(),
        resourceValue,
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
        resourceKey.Value(),
        resourceValue);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    if (!explicitKey &&
        object.type == Aero::Style::StaticTypeId() &&
        object.object) {
        const auto& style = static_cast<const Aero::Style&>(
            *object.object);
        const Meta::TypeInfo* targetType =
            schema_->Types().FindType(
                style.GetTargetType());
        if (targetType != nullptr &&
            !targetType->Name().Empty()) {
            Base::String alias;
            Base::Result<void> named =
                alias.Assign("Style.");
            if (named) {
                named = alias.Append(
                    targetType->Name());
            }
            if (!named) return named.GetStatus();
            if (!scope.resources.Contains(alias.View())) {
                Base::Result<Aero::ResourceKey> aliasKey =
                    Aero::ResourceKey::FromString(
                        alias.View());
                if (!aliasKey) return aliasKey.GetStatus();
                Base::Result<void> localAlias =
                    scope.resources.Add(
                        aliasKey.Value(),
                        resourceValue,
                        source);
                if (!localAlias) {
                    return Failure(
                        localAlias.GetStatus(),
                        XamlObjectWriterDiagnosticCodes::
                            ResourceRegistrationFailed,
                        MessageResourceRegistrationFailed,
                        source);
                }
                Base::Result<void> callbackAlias =
                    schema_->AddResource(
                        owner.type,
                        *owner.object,
                        aliasKey.Value(),
                        resourceValue);
                if (!callbackAlias) {
                    return Failure(
                        callbackAlias.GetStatus(),
                        XamlObjectWriterDiagnosticCodes::
                            ResourceRegistrationFailed,
                        MessageResourceRegistrationFailed,
                        source);
                }
            }
        }
    }

    object.resourceRegistered = true;
    return true;
}

Base::Result<Aero::ResourceValue> ObjectBuilder::LookupResource(
    Base::StringView key) const noexcept {
    constexpr Base::StringView PresentationNamespace(
        "http://schemas.microsoft.com/winfx/2006/xaml/presentation");

    Aero::ResourceKey candidates[4];
    std::uint32_t candidateCount = 0U;
    auto addCandidate = [&](Aero::ResourceKey candidate) noexcept {
        if (!candidate.IsValid() || candidateCount >= 4U) {
            return;
        }
        for (std::uint32_t index = 0U; index < candidateCount; ++index) {
            if (candidates[index] == candidate) {
                return;
            }
        }
        candidates[candidateCount++] = std::move(candidate);
    };
    auto addStringKey = [&](Base::StringView text) noexcept {
        if (text.Empty()) {
            return;
        }
        Base::Result<Aero::ResourceKey> parsed =
            Aero::ResourceKey::FromString(text);
        if (parsed) {
            addCandidate(std::move(parsed).Value());
        }
    };
    auto addTypeCandidates = [&](
        Base::StringView localName,
        Meta::TypeId type) noexcept {
        if (type != Meta::InvalidTypeId) {
            addCandidate(Aero::ResourceKey::FromType(type));
        }
        if (!localName.Empty()) {
            Base::String alias;
            if (alias.Assign("Style.") && alias.Append(localName)) {
                addStringKey(alias.View());
            }
            addStringKey(localName);
        }
    };
    auto splitTypeName = [&](Base::StringView typeName)
        -> Base::Result<std::pair<Base::StringView, Base::StringView>> {
        std::uint32_t colon = typeName.SizeBytes();
        for (std::uint32_t index = 0U;
             index < typeName.SizeBytes();
             ++index) {
            if (typeName[index] != ':') continue;
            if (colon != typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key contains multiple namespace prefixes");
            }
            colon = index;
        }
        Base::StringView prefix;
        Base::StringView localName = typeName;
        if (colon != typeName.SizeBytes()) {
            if (colon == 0U || colon + 1U >= typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key namespace prefix is malformed");
            }
            prefix = typeName.Substr(0U, colon);
            localName = typeName.Substr(
                colon + 1U,
                typeName.SizeBytes() - colon - 1U);
        }
        return std::make_pair(prefix, localName);
    };
    auto resolveTypeId = [&](Base::StringView prefix, Base::StringView localName)
        -> Base::Result<Meta::TypeId> {
        Base::StringView namespaceUri = PresentationNamespace;
        Base::Result<Base::StringView> bound = LookupNamespace(prefix);
        if (bound) {
            namespaceUri = bound.Value();
        } else if (!prefix.Empty()) {
            return bound.GetStatus();
        }
        Base::Result<const Meta::TypeInfo*> type =
            schema_->ResolveType(namespaceUri, localName);
        if (!type && prefix.Empty() &&
            namespaceUri != PresentationNamespace) {
            type = schema_->ResolveType(PresentationNamespace, localName);
        }
        if (!type) {
            return type.GetStatus();
        }
        return type.Value()->Id();
    };
    auto looksLikeTypeName = [&](Base::StringView text) noexcept -> bool {
        if (text.Empty() || text[0] == '{') {
            return false;
        }
        bool sawColon = false;
        for (std::uint32_t index = 0U; index < text.SizeBytes(); ++index) {
            const char character = text[index];
            if (character == ':') {
                if (sawColon || index == 0U ||
                    index + 1U >= text.SizeBytes()) {
                    return false;
                }
                sawColon = true;
                continue;
            }
            const bool letter =
                (character >= 'A' && character <= 'Z') ||
                (character >= 'a' && character <= 'z') ||
                character == '_';
            const bool digit =
                character >= '0' && character <= '9';
            if (!(letter || (index > 0U && digit))) {
                return false;
            }
        }
        return true;
    };

    Base::StringView extensionName;
    Base::StringView typeArgument;
    const MarkupValueKind markup =
        ParseMarkupValue(key, extensionName, typeArgument);
    if (markup == MarkupValueKind::Extension &&
        extensionName == Base::StringView("x:Type")) {
        Base::Result<std::pair<Base::StringView, Base::StringView>> parts =
            splitTypeName(typeArgument);
        if (!parts) {
            return parts.GetStatus();
        }
        Base::Result<Meta::TypeId> typeId =
            resolveTypeId(parts.Value().first, parts.Value().second);
        if (typeId) {
            addTypeCandidates(parts.Value().second, typeId.Value());
        }
        addStringKey(key);
    } else {
        addStringKey(key);
        if (looksLikeTypeName(key)) {
            Base::Result<std::pair<Base::StringView, Base::StringView>> parts =
                splitTypeName(key);
            if (parts) {
                Base::Result<Meta::TypeId> typeId =
                    resolveTypeId(parts.Value().first, parts.Value().second);
                if (typeId) {
                    addTypeCandidates(parts.Value().second, typeId.Value());
                }
            }
        }
    }
    if (candidateCount == 0U) {
        addStringKey(key);
        if (candidateCount == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "String resource key cannot be empty");
        }
    }

    auto lookupOne = [&](const Aero::ResourceKey& resourceKey)
        -> Base::Result<Aero::ResourceValue> {
        for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
            const Frame& frame = frames_[index - 1U];
            if (frame.kind != FrameKind::Object ||
                frame.resourceScopeIndex == InvalidIndex ||
                frame.resourceScopeIndex >= resourceScopes_.Size()) {
                continue;
            }
            Base::Result<Aero::ResourceValue> value =
                (resourceScopes_[frame.resourceScopeIndex].external != nullptr
                    ? resourceScopes_[frame.resourceScopeIndex].external
                    : &resourceScopes_[frame.resourceScopeIndex].resources)
                    ->Lookup(resourceKey);
            if (value) {
                return value;
            }
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        if (Base::Object* deferredOwner = FindDeferredContentOwner()) {
            Aero::ResourceDictionary* templateResources = nullptr;
            const Meta::TypeId ownerType = deferredOwner->RuntimeType();
            if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||
                ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<::Aero::DataTemplate&>(
                        *deferredOwner).GetResources();
            } else if (
                ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<Controls::ItemsPanelTemplate&>(
                        *deferredOwner).GetResources();
            } else if (
                ownerType == Controls::ControlTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<Controls::ControlTemplate&>(
                        *deferredOwner).GetResources();
            }
            if (templateResources != nullptr) {
                Base::Result<Aero::ResourceValue> value =
                    templateResources->Lookup(resourceKey);
                if (value) return value;
                if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                    return value.GetStatus();
                }
            }
        }
        // WPF: a later merged dictionary's StaticResource sees keys from earlier
        // application merged dictionaries AND from nested MergedDictionaries, even
        // while this document still has an object/member frame on the stack.
        for (std::uint32_t index = resourceScopes_.Size();
             index > 0U; --index) {
            const ResourceScopeRecord& scope =
                resourceScopes_[index - 1U];
            const Aero::ResourceDictionary* dictionary =
                scope.external != nullptr
                    ? scope.external
                    : &scope.resources;
            Base::Result<Aero::ResourceValue> value =
                dictionary->Lookup(resourceKey);
            if (value) return value;
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        {
            Base::Result<Aero::ResourceValue> committed =
                committedResources_.Lookup(resourceKey);
            if (committed) return committed;
            if (committed.GetStatus().code != Base::ErrorCode::NotFound) {
                return committed.GetStatus();
            }
        }
        if (loadContext_ != nullptr && loadContext_->resources != nullptr) {
            Base::Result<Aero::ResourceValue> value =
                loadContext_->resources->Lookup(resourceKey);
            if (value) {
                return value;
            }
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        if (loadContext_ != nullptr &&
            loadContext_->fallbackResources != nullptr &&
            loadContext_->fallbackResources != loadContext_->resources) {
            Base::Result<Aero::ResourceValue> value =
                loadContext_->fallbackResources->Lookup(resourceKey);
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
    };

    Base::Result<Aero::ResourceValue> last =
        Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageStaticResourceNotFound.Data());
    for (std::uint32_t index = 0U; index < candidateCount; ++index) {
        Base::Result<Aero::ResourceValue> value =
            lookupOne(candidates[index]);
        if (value) {
            return value;
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
        last = std::move(value);
    }
    return last;
}

Base::Result<void> ObjectBuilder::CreateScopesForObject(
    std::uint32_t objectIndex,
    Frame& frame,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const bool documentRoot = objectIndex == rootObjectIndex_;
    if (documentRoot || schema_->CreatesNameScope(created_[objectIndex].type)) {
        NameScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        const std::uint32_t index = nameScopes_.Size();
        Base::Result<void> appendResult =
            nameScopes_.PushBack(std::move(scope));
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
        ResourceScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        scope.external = schema_->ResolveResourceScope(
            created_[objectIndex].type,
            *created_[objectIndex].object);
        const std::uint32_t index = resourceScopes_.Size();
        Base::Result<void> appendResult =
            resourceScopes_.PushBack(std::move(scope));
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

ExtensionServices ObjectBuilder::BuildExtensionServices(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    ExtensionServices services;
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
    if (loadContext_ != nullptr) {
        services.templatedParent = loadContext_->templatedParent;
        services.baseUri = loadContext_->baseUri;
        services.effectiveValues = loadContext_->effectiveValues;
        services.bindings = loadContext_->bindings;
        services.fallbackResources = loadContext_->fallbackResources;
    }
    services.source = source;
    services.nameScope = FindActiveNameScope();
    services.namespaces = NamespaceScope(
        &ObjectBuilder::NamespaceLookupCallback,
        this);
    services.resources = ResourceResolver(
        &ObjectBuilder::ResourceLookupCallback,
        this);
    serviceResourceChain_.Clear();
    for (std::uint32_t index = frames_.Size();
         index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.resourceScopeIndex == InvalidIndex ||
            frame.resourceScopeIndex >= resourceScopes_.Size()) {
            continue;
        }
        const Aero::ResourceDictionary* dictionary =
            resourceScopes_[frame.resourceScopeIndex].external;
        if (dictionary == nullptr) {
            dictionary =
                &resourceScopes_[
                    frame.resourceScopeIndex].resources;
        }
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == dictionary) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(dictionary);
            if (!added) {
                serviceResourceChain_.Clear();
                break;
            }
        }
    }
    if (loadContext_ != nullptr &&
        loadContext_->resources != nullptr) {
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == loadContext_->resources) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(
                    loadContext_->resources);
            if (!added) {
                serviceResourceChain_.Clear();
            }
        }
    }
    services.ambientResourceChain = {
        serviceResourceChain_.Data(),
        serviceResourceChain_.Size()};
    services.visualContent = &resultVisualContent_;
    services.deferredContentOwner =
        FindDeferredContentOwner();
    services.deferredContent =
        &deferredContent_;
    return services;
}

const Aero::NameScope*
ObjectBuilder::FindActiveNameScope() const noexcept {
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

Base::Object*
ObjectBuilder::FindDeferredContentOwner() const noexcept {
    for (std::uint32_t index = frames_.Size();
         index > 0U;
         --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.objectIndex >= created_.Size()) {
            continue;
        }
        const CreatedObjectRecord& record =
            created_[frame.objectIndex];
        if (schema_->DefersVisualContent(
                record.type)) {
            return record.object.Get();
        }
    }
    return nullptr;
}

std::uint32_t ObjectBuilder::FindNameScopeIndexForObject(
    std::uint32_t objectIndex) const noexcept {
    const std::uint32_t objectFrame = FindObjectFrameIndex(objectIndex);
    if (objectFrame == InvalidIndex) {
        return InvalidIndex;
    }
    for (std::uint32_t index = objectFrame + 1U; index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.nameScopeIndex == InvalidIndex) {
            continue;
        }
        // UserControl/Page own a nested NameScope for names inside their
        // content. Their own x:Name belongs to the enclosing namescope so
        // sibling ElementName bindings (MultiBinding ElementName=BgR) resolve.
        if (frame.objectIndex == objectIndex &&
            frame.nameScopeIndex != documentNameScopeIndex_) {
            continue;
        }
        return frame.nameScopeIndex;
    }
    return documentNameScopeIndex_;
}

std::uint32_t ObjectBuilder::FindResourceScopeIndexForParent() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.resourceScopeIndex != InvalidIndex) {
            return frame.resourceScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t ObjectBuilder::FindObjectFrameIndex(
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

ObjectBuilder::MarkupValueKind ObjectBuilder::ParseMarkupValue(
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

    std::uint32_t nestedDepth = 0U;
    char quote = '\0';
    for (char character : inner) {
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
        } else if (character == '{') {
            ++nestedDepth;
        } else if (character == '}') {
            if (nestedDepth == 0U) {
                return MarkupValueKind::Invalid;
            }
            --nestedDepth;
        }
    }
    if (nestedDepth != 0U || quote != '\0') {
        return MarkupValueKind::Invalid;
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
        constexpr Base::StringView resourceKeyPrefix("ResourceKey=");
        if (argument.SizeBytes() > resourceKeyPrefix.SizeBytes() &&
            argument.Substr(0U, resourceKeyPrefix.SizeBytes()) ==
                resourceKeyPrefix) {
            argument = TrimAscii(argument.Substr(
                resourceKeyPrefix.SizeBytes(),
                argument.SizeBytes() - resourceKeyPrefix.SizeBytes()));
            if (argument.Empty()) return MarkupValueKind::Invalid;
        }
        return MarkupValueKind::StaticResource;
    }
    return MarkupValueKind::Extension;
}

Base::Result<ProvidedValue> ObjectBuilder::EvaluateMarkupExtension(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Base::StringView extensionName,
    Base::StringView arguments,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
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
    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            namespaceResult.Value(),
            localName);
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<ProvidedValue> provided =
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

ObjectBuilder::AssignmentRecord* ObjectBuilder::FindAssignment(
    std::uint32_t objectIndex,
    Meta::MemberId member) noexcept {
    for (AssignmentRecord& assignment : assignments_) {
        if (assignment.objectIndex == objectIndex &&
            assignment.member == member) {
            return &assignment;
        }
    }
    return nullptr;
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

Base::Result<Base::StringView> ObjectBuilder::NamespaceLookupCallback(
    void* context,
    Base::StringView prefix) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageNamespaceState.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupNamespace(prefix);
}

Base::Result<Aero::ResourceValue> ObjectBuilder::ResourceLookupCallback(
    void* context,
    Base::StringView key) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageStaticResourceNotFound.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupResource(key);
}

} // namespace Aero::Markup


// ===== ObjectWriter =====




namespace Aero::Markup {
namespace {

Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> DeferredContentPlan::Stage(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child || member == Meta::InvalidMemberId ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML content edge is invalid");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            false});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        metadata.WriteContent(parent, member, child);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageProperty(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child ||
        member == Meta::InvalidMemberId) {
        return InvalidContentState(
            "Deferred XAML structural property edge is invalid");
    }
    const Meta::PropertyInfo* property =
        metadata.Types().FindProperty(member);
    if (property == nullptr) {
        return InvalidContent(
            "Deferred XAML structural property was not found");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            true});
    if (!retained) return retained.GetStatus();
    const Meta::Value value =
        Meta::Value::FromObject(
            property->ValueType(), child);
    Base::Result<void> written =
        metadata.SetProperty(
            parent, member, value);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::CopyForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredContentEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredContentEdge& edge : edges_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageBinding(
    Base::Object& owner,
    Base::Object* source,
    Base::StringView sourceName,
    Base::StringView relativeAncestorType,
    std::uint32_t relativeAncestorLevel,
    ::Aero::DependencyObject& target,
    ::Aero::Meta::Registry& metadata,
    Meta::DependencyPropertyHandle targetProperty,
    Meta::DependencyPropertyHandle dataContextProperty,
    Base::StringView path,
    Base::StringView stringFormat,
    Data::BindingMode mode,
    Meta::UpdateSourceTrigger updateSourceTrigger,
    bool bindsToSource,
    const Base::Ref<Data::IValueConverter>& converter,
    const Meta::PropertyValue& converterParameter) noexcept {
    if (!targetProperty.IsValid() ||
        (path.Empty() && !bindsToSource) ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML Binding declaration is invalid");
    }
    DeferredBindingEdge edge;
    edge.owner = &owner;
    edge.source = source;
    Base::Result<void> sourceAssigned =
        edge.sourceName.Assign(sourceName);
    if (!sourceAssigned) return sourceAssigned.GetStatus();
    sourceAssigned = edge.relativeAncestorType.Assign(
        relativeAncestorType);
    if (!sourceAssigned) return sourceAssigned.GetStatus();
    edge.relativeAncestorLevel = relativeAncestorLevel;
    edge.target = &target;
    edge.metadata = &metadata;
    edge.targetProperty = targetProperty;
    edge.dataContextProperty = dataContextProperty;
    edge.mode = mode;
    edge.bindsToSource = bindsToSource;
    edge.updateSourceTrigger = updateSourceTrigger;
    edge.converter = converter;
    edge.converterParameter = converterParameter;
    Base::Result<void> assigned =
        edge.path.Assign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = edge.stringFormat.Assign(
        stringFormat);
    if (!assigned) return assigned.GetStatus();
    return bindings_.PushBack(std::move(edge));
}

Base::Result<void>
DeferredContentPlan::CopyBindingsForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredBindingEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredBindingEdge& edge : bindings_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

void DeferredContentPlan::ReleaseOwner(
    Base::Object& owner) noexcept {
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner != &owner) continue;
        bool firstForParent = true;
        for (std::uint32_t earlier = 0U;
             earlier < index;
             ++earlier) {
            firstForParent =
                firstForParent &&
                (edges_[earlier].owner != &owner ||
                 edges_[earlier].parent !=
                     edge.parent ||
                 (edge.property &&
                  edges_[earlier].member !=
                      edge.member));
        }
        if (firstForParent &&
            edge.parent != nullptr &&
            edge.metadata != nullptr) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parent,
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parent,
                    edge.member);
            }
        }
    }

    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            edges_[output] = std::move(edge);
        }
        ++output;
    }
    (void)edges_.Resize(output);

    output = 0U;
    for (std::uint32_t index = 0U;
         index < bindings_.Size();
         ++index) {
        DeferredBindingEdge& edge = bindings_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            bindings_[output] = std::move(edge);
        }
        ++output;
    }
    (void)bindings_.Resize(output);
}

void DeferredContentPlan::ReleaseAll() noexcept {
    while (!edges_.Empty() || !bindings_.Empty()) {
        Base::Object* owner = !edges_.Empty()
            ? edges_.Front().owner
            : bindings_.Front().owner;
        if (owner == nullptr) {
            edges_.Clear();
            return;
        }
        ReleaseOwner(*owner);
    }
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

Base::Result<Aero::Media::Visual*> ObjectWriter::ResolveVisual(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    if (!schema.Types().IsDerivedFrom(object.RuntimeType(), type) ||
        !schema.Types().IsDerivedFrom(
            type, Aero::Media::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Aero::Media::Visual*>(&object);
}

Base::Result<Aero::UIElement*> ObjectWriter::ResolveUIElement(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    Base::Result<Aero::Media::Visual*> visual =
        ResolveVisual(schema, object, type);
    if (!visual) return visual.GetStatus();
    Aero::UIElement* element =
        ::Aero::TryCast<::Aero::UIElement>(visual.Value());
    if (element == nullptr) {
        return InvalidContent("XAML object is not a UIElement");
    }
    return element;
}

Base::Result<void> ObjectWriter::StageContent(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    const Meta::Value& value,
    const ExtensionServices& services) noexcept {
    VisualContentPlan* plan = services.visualContent;
    if (services.targetObject != &object ||
        value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return InvalidContentState(
            "XAML content requires a non-null object");
    }

    ::Aero::Meta::Registry* metadata = schema.Metadata();
    if (metadata == nullptr) {
        return InvalidContentState(
            "XAML content metadata is unavailable");
    }
    Base::Result<Meta::ContentInfo> contentResult =
        metadata->GetContentInfo(services.targetMember);
    const Meta::PropertyInfo* property =
        schema.Types().FindProperty(
            services.targetMember);
    const bool attachedMember =
        property != nullptr &&
        (static_cast<std::uint32_t>(property->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Attached)) != 0U;
    const bool structuralProperty =
        property != nullptr &&
        (static_cast<std::uint32_t>(
             property->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        metadata->CanWriteProperty(
            services.targetMember);
    if (!contentResult && !structuralProperty) {
        return InvalidContent(
            "XAML content target has no content metadata");
    }
    if (!structuralProperty) {
        const Meta::ContentInfo& content =
            contentResult.Value();
        if (!content.writable ||
            !content.clearable ||
            (!attachedMember &&
             !schema.Types().IsDerivedFrom(
                 services.targetObjectType,
                 content.ownerType)) ||
            (services.deferredContentOwner == nullptr &&
             !content.IsVisual())) {
            return InvalidContent(
                "XAML content target has no compatible content facet");
        }
    }

    Base::Object* childObject = value.AsObject().Get();
    const bool childIsDependencyObject =
        schema.Types().IsDerivedFrom(
            childObject->RuntimeType(),
            Aero::DependencyObject::StaticTypeId());

    // A deferred template owns a visual root without itself being a ::Aero::Media::Visual.
    // Commit that root through the template's content accessor; descendant
    // visual edges are staged below once their actual visual parent exists.
    if (services.deferredContentOwner == &object &&
        !schema.Types().IsDerivedFrom(
            services.targetObjectType,
            Aero::Media::Visual::StaticTypeId())) {
        if (structuralProperty) {
            return InvalidContent(
                "A non-visual template root cannot use a visual structural property");
        }
        return metadata->WriteContent(
            object,
            services.targetMember,
            value.AsObject());
    }

    if (services.deferredContentOwner != nullptr) {
        const bool parentIsVisual =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Aero::Media::Visual::StaticTypeId());
        const bool parentIsFreezable =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Aero::Freezable::StaticTypeId());
        const bool parentIsTimeline =
            schema.Types().IsDerivedFrom(
                object.RuntimeType(),
                Media::Animation::Timeline::StaticTypeId());
        const bool deferParent =
            services.deferredContentOwner == &object ||
            parentIsVisual ||
            (parentIsFreezable && !parentIsTimeline);
        if (!deferParent) {
            // DataTemplate.Resources Storyboards and Trigger.EnterActions are
            // live shared objects. Visual-tree Freezables (TransformGroup)
            // still clone through the deferred graph.
            return metadata->WriteContent(
                object,
                services.targetMember,
                value.AsObject());
        }
        if (services.deferredContent == nullptr ||
            !childIsDependencyObject) {
            return InvalidContentState(
                "Deferred XAML content requires a DependencyObject child");
        }
        return structuralProperty
            ? services.deferredContent->
                  StageProperty(
                      *services.deferredContentOwner,
                      object,
                      value.AsObject(),
                      *metadata,
                      services.targetMember)
            : services.deferredContent->Stage(
                  *services.deferredContentOwner,
                  object,
                  value.AsObject(),
                  *metadata,
                  services.targetMember);
    }

    if (plan == nullptr) {
        return InvalidContentState(
            "XAML visual content plan is unavailable");
    }
    Base::Result<Aero::UIElement*> childResult =
        ResolveUIElement(schema, *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();
    Base::Result<Aero::UIElement*> parentResult =
        ResolveUIElement(
            schema, object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    Base::Result<void> reserved = plan->Reserve(
        plan->contentEdges.Size() + 1U,
        plan->mountEdges.Size() + 1U,
        plan->nodes.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Aero::Media::Visual*> parentNode =
        ResolveVisual(
            schema, object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Aero::Media::Visual*> childNode =
        ResolveVisual(schema, *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded =
        plan->AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded =
        plan->AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked =
        plan->contentEdges.PushBack({
            std::move(parentOwner), value.AsObject(),
            metadata, services.targetMember,
            structuralProperty});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.PushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written =
        structuralProperty
        ? metadata->SetProperty(
              object,
              services.targetMember,
              value)
        : metadata->WriteContent(
              object,
              services.targetMember,
              value.AsObject());
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup

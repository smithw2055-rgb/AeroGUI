#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"
#include "gui/markup/XamlObjectWriterCommon.hpp"
#include "gui/markup/MarkupExtensionHost.hpp"
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/ServiceProvider.hpp>
#include <Aero/VisualStateManager.hpp>

// ===== ObjectWriter core / node stack =====

namespace Aero::Markup {
// ===== ObjectWriter load lifecycle (ctor/Load/Complete/Create) =====

ObjectWriter::ObjectWriter(
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

ObjectWriter::~ObjectWriter() noexcept {
    AbortTransaction();
}

Base::Result<LoaderResult> ObjectWriter::Load(
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

Base::Result<LoaderResult> ObjectWriter::Load(
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

Base::Result<LoaderResult> ObjectWriter::Load(
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

Base::Result<LoaderResult> ObjectWriter::Load(
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

Base::Result<LoaderResult> ObjectWriter::CompleteLoad(
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

Base::Result<void> ObjectWriter::ResolveDeferredStaticResources() noexcept {
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

Base::Result<void> ObjectWriter::FinalizeDeferredStyles() noexcept {
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

Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadReaderCore(
    NodeReader& reader) noexcept {
    StreamingXamlNodeCursor cursor(reader);
    return LoadCursorCore(cursor);
}

Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadCursorCore(
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
            loadContext_->recordingNodes->PushBack(
                std::move(cloned).Value());
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

Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadCompiledCore(
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

Base::Result<Base::Ref<Base::Object>> ObjectWriter::CreateObject(
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

} // namespace Aero::Markup

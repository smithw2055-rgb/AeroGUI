#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Gui.hpp>
#include "gui/GuiRuntime.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Events.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/View.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Markup {
// ============================================================================
// SECTION: ObjectWriter Declaration
// ============================================================================

class CompiledDocument;
class NodeReader;

namespace XamlObjectWriterDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnknownMember =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 203U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode UnsupportedMember =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 205U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidValue =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 206U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidWriterState =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 207U);
} // namespace XamlObjectWriterDiagnosticCodes

class ObjectWriter {
public:
    explicit ObjectWriter(
        Schema& schema,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    ~ObjectWriter() noexcept;

    ObjectWriter(const ObjectWriter&) = delete;
    ObjectWriter& operator=(const ObjectWriter&) = delete;

    Base::Result<LoaderResult> Load(
        const CompiledDocument& document,
        const LoadState& context) noexcept;

    bool IsConsumed() const noexcept {
        return consumed_;
    }

    Schema& GetSchema() const noexcept {
        return *schema_;
    }
    Diagnostics::IDiagnosticSink* Diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    Markup::Schema* schema_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    Base::Vector<CommittedEffect> extensionEffects_;
    Aero::NameScope committedNames_;
    Aero::ResourceDictionary committedResources_;
    VisualContentPlan resultVisualContent_;
    Base::Ref<Base::Object> root_;
    bool consumed_ = false;
    bool hasDeferredStaticResources_ = false;
    bool finalizedByVM_ = false;
    Base::ResourceUri finalizedCanonicalUri_;
    Base::Vector<Base::ResourceUri> finalizedDependencies_;
    const LoadState* loadContext_ = nullptr;

    Base::Result<LoaderResult> CompleteLoad(
        Base::Result<Base::Ref<Base::Object>> loaded) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCompiledCore(
        const CompiledDocument& document) noexcept;
    Base::Result<void> ConnectEvent(
        Base::Object& eventSourceObject,
        Meta::TypeId eventSourceType,
        Base::Object& codeBehindObject,
        Meta::TypeId codeBehindType,
        Meta::MemberId eventMemberId,
        Base::StringView handlerName,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
    void AbortTransaction() noexcept;
    void ClearTransaction() noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode diagnostic,
        Base::StringView message,
        ::Aero::Diagnostics::SourceSpan source) noexcept;
};

namespace WriterSupport {
inline constexpr Base::StringView MessageSchemaNotReady(
    "XAML object writer requires a frozen schema context");
inline constexpr Base::StringView MessageUnknownMember(
    "XAML member does not resolve on the target object type");
inline constexpr Base::StringView MessageUnsupportedMember(
    "XAML member has no supported object-writer adapter");
inline constexpr Base::StringView MessageInvalidValue(
    "XAML value conversion or member assignment failed");

inline Base::Status SessionConsumedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML load session is single use");
}
} // namespace WriterSupport
} // namespace Aero::Markup

// ============================================================================
// SECTION: ObjectWriter Implementation
// ============================================================================

// ===== XamlObjectWriter.cpp =====

// ServiceProvider.hpp inlined into MarkupExtensionContract.hpp

// ===== ObjectWriter / DeferredContentPlan =====
namespace Aero::Markup {
using namespace WriterSupport;
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
    edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            false});
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
    edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            true});
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
        output.PushBack(edge);
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
    bindings_.PushBack(std::move(edge));
    return {};
}

Base::Result<void>
DeferredContentPlan::CopyBindingsForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredBindingEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredBindingEdge& edge : bindings_) {
        if (edge.owner != &owner) continue;
        output.PushBack(edge);
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

} // namespace Aero::Markup

// ===== Scopes & ObjectWriter load lifecycle =====
namespace Aero::Markup {
using namespace WriterSupport;
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

ObjectWriter::ObjectWriter(
    ::Aero::Markup::Schema& schema,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics) {}

ObjectWriter::~ObjectWriter() noexcept {
    AbortTransaction();
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
    if (finalizedByVM_) {
        result.canonicalUri = std::move(finalizedCanonicalUri_);
        result.dependencies = std::move(finalizedDependencies_);
    } else if (loadContext_ != nullptr &&
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

Base::Result<Base::Ref<Base::Object>> ObjectWriter::LoadCompiledCore(
    const CompiledDocument& document) noexcept {
    if (consumed_) return SessionConsumedStatus();
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

    // High-speed Single-Track XamlVM Bytecode Document Execution pipeline
    Base::Result<const XamlBytecodeProgram*> bytecodeReady = EnsureDocumentBytecode(
        document, *schema_);
    if (!bytecodeReady) {
        return bytecodeReady.GetStatus();
    }
    if (bytecodeReady.Value() == nullptr || !bytecodeReady.Value()->IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Failed to compile XAML document to valid bytecode");
    }

    const XamlBytecodeProgram& program = *bytecodeReady.Value();
    XamlDocumentExecutionContext vmContext;
    vmContext.schema = schema_;
    vmContext.loadContext = loadContext_;
    vmContext.runtime = schema_->Metadata();
    XamlDocumentExecutionResult vmResult;
    Base::Result<void> executed = XamlVM::ExecuteDocument(
        program, vmContext, vmResult);
    if (!executed) {
        return executed.GetStatus();
    }
    if (!vmResult.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Executing XAML bytecode produced null root");
    }

    consumed_ = true;
    root_ = std::move(vmResult.root);
    committedNames_ = std::move(vmResult.names);
    committedResources_ = std::move(vmResult.resources);
    resultVisualContent_ = std::move(vmResult.visualContent);
    extensionEffects_ = std::move(vmResult.effects.Items());
    hasDeferredStaticResources_ = vmResult.hasDeferredStaticResources;
    finalizedByVM_ = vmResult.finalizedByVM;
    finalizedCanonicalUri_ = std::move(vmResult.canonicalUri);
    finalizedDependencies_ = std::move(vmResult.dependencies);
    Base::Ref<Base::Object> result = std::move(root_);
    return result;
}
} // namespace Aero::Markup

// ===== ObjectWriter transaction & event helpers =====
namespace Aero::Markup {
using namespace WriterSupport;

Base::Result<void> ObjectWriter::ConnectEvent(
    Base::Object& eventSourceObject,
    Meta::TypeId eventSourceType,
    Base::Object& codeBehindObject,
    Meta::TypeId codeBehindType,
    Meta::MemberId eventMemberId,
    Base::StringView handlerName,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (handlerName.Empty() || eventMemberId == Meta::InvalidMemberId) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event attribute requires a handler name"),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            source);
    }

    Meta::EventHandlerThunk thunk =
        schema_->Metadata()->Types().FindEventHandler(
            codeBehindType,
            handlerName);
    if (thunk == nullptr) {
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

    Base::Ref<Base::Object> codeBehindRef =
        Base::Ref<Base::Object>::TryFromBorrowed(codeBehindObject);
    if (!codeBehindRef) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML code-behind object is not reference-counted");
    }
    Base::Result<Base::Ref<XamlEventConnection>> connection =
        Base::MakeRef<XamlEventConnection>(
            Base::WeakRef<Base::Object>(codeBehindRef),
            thunk);
    if (!connection) return connection.GetStatus();
    Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(
        XamlEventInvoker{std::move(connection).Value()});
    const RoutedEventHandle routedEvent{eventMemberId};

    Base::Result<void> connected;
    if (schema_->Types().IsDerivedFrom(
            eventSourceType, UIElement::StaticTypeId())) {
        static_cast<UIElement*>(&eventSourceObject)->AddHandler(
            routedEvent, handler);
    } else if (schema_->Types().IsDerivedFrom(
                   eventSourceType,
                   ContentElement::StaticTypeId())) {
        static_cast<ContentElement*>(&eventSourceObject)->AddHandler(
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

void ObjectWriter::AbortTransaction() noexcept {
    root_.Reset();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
    for (std::uint32_t index = extensionEffects_.Size();
         index > 0U; --index) {
        extensionEffects_[index - 1U].Rollback();
    }
    extensionEffects_.Clear();
    ClearTransaction();
}

void ObjectWriter::ClearTransaction() noexcept {
    extensionEffects_.Clear();
    root_.Reset();
    hasDeferredStaticResources_ = false;
}

Base::Status ObjectWriter::Failure(
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
} // namespace Aero::Markup

// ============================================================================
// SECTION: ObjectLoader Implementation
// ============================================================================

// ===== LoaderResult =====

namespace Aero::Markup {
Base::Result<void> VisualContentPlan::Reserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    contentEdges.Reserve(contentEdgeCount);
    mountEdges.Reserve(mountEdgeCount);
    nodes.Reserve(nodeCount);
    return {};
}

Base::Result<void> VisualContentPlan::AddNode(
    Aero::Media::Visual& node) noexcept {
    for (Aero::Media::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    nodes.PushBack(&node);
    return {};
}

void VisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        VisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() ==
                    edge.parentOwner.Get() &&
                (!edge.property ||
                 contentEdges[prior].member ==
                     edge.member)) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.metadata != nullptr && edge.parentOwner) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parentOwner.Get(),
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parentOwner.Get(),
                    edge.member);
            }
        }
    }
}

void VisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}
} // namespace Aero::Markup

// ===== Loader =====

namespace Aero::Markup {
namespace LoaderDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidUri =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 301U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode XamlProviderNotFound =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 302U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceLoadFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 303U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceRejected =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 304U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode RecursiveLoad =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 305U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode LoadComponentTypeMismatch =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 306U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode ResourceDependencyFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 307U);
} // namespace LoaderDiagnosticCodes

struct Loader::LoaderState {
    LoaderState(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const LoadState* loadState = nullptr) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          loadState_(loadState) {}

    struct Operation;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* loadState_ = nullptr;
};

using Aero::ResourceDictionary;

namespace {
class MemoryStream : public Base::Stream {
public:
    explicit MemoryStream(
        Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    bool CanRead() const noexcept override { return true; }

    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t available = bytes_.Size() - position_;
        const std::uint32_t count =
            std::min(available, destination.Size());
        if (count != 0U) {
            std::memcpy(
                destination.Data(),
                bytes_.Data() + position_,
                count);
            position_ += count;
        }
        return count;
    }

    bool CanSeek() const noexcept override { return true; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        return static_cast<std::uint64_t>(position_);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return static_cast<std::uint64_t>(bytes_.Size());
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        const std::int64_t base = origin == Base::SeekOrigin::Begin
            ? 0
            : origin == Base::SeekOrigin::Current
                ? static_cast<std::int64_t>(position_)
                : static_cast<std::int64_t>(bytes_.Size());
        const std::int64_t next = base + offset;
        if (next < 0 || static_cast<std::uint64_t>(next) > bytes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Memory stream seek is outside its bounds");
        }
        position_ = static_cast<std::uint32_t>(next);
        return static_cast<std::uint64_t>(position_);
    }

private:
    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t position_ = 0U;
};

class FileStream : public Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() noexcept override {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    bool CanRead() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        if (destination.Empty()) return std::uint32_t{0U};
        const std::size_t count = std::fread(
            destination.Data(), 1U, destination.Size(), file_);
        if (count == 0U && std::ferror(file_) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream read failed");
        }
        return static_cast<std::uint32_t>(count);
    }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        const long position = std::ftell(file_);
        if (position < 0L) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream position could not be read");
        }
        return static_cast<std::uint64_t>(position);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return length_;
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        if (file_ == nullptr ||
            offset < static_cast<std::int64_t>(std::numeric_limits<long>::min()) ||
            offset > static_cast<std::int64_t>(std::numeric_limits<long>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "File stream seek is outside its bounds");
        }
        const int whence = origin == Base::SeekOrigin::Begin
            ? SEEK_SET
            : origin == Base::SeekOrigin::Current
                ? SEEK_CUR
                : SEEK_END;
        if (std::fseek(file_, static_cast<long>(offset), whence) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream seek failed");
        }
        return Position();
    }

private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

class HashingStream : public Base::Stream {
public:
    explicit HashingStream(Base::Stream& source) noexcept
        : source_(&source) {}

    bool CanRead() const noexcept override {
        return source_ != nullptr && source_->CanRead();
    }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (source_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Hashing stream is not initialized");
        }
        Base::Result<std::uint32_t> result =
            source_->Read(destination);
        if (!result || result.Value() == 0U) return result;
        constexpr Base::HashCode Prime = UINT64_C(1099511628211);
        for (std::uint32_t index = 0U;
             index < result.Value(); ++index) {
            hash_ ^= static_cast<Base::HashCode>(
                destination[index]);
            hash_ *= Prime;
        }
        size_ += result.Value();
        return result;
    }
    bool CanSeek() const noexcept override { return false; }
    Base::HashCode Hash() const noexcept {
        return Base::MixHash64(hash_ ^ size_);
    }

private:
    Base::Stream* source_ = nullptr;
    Base::HashCode hash_ =
        UINT64_C(14695981039346656037) ^
        Base::MixHash64(0U);
    std::uint64_t size_ = 0U;
};

char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

Base::Result<void> AssignLowerAscii(
    Base::String& output,
    Base::StringView value) noexcept {
    Base::String replacement(&output.Allocator());
    replacement.Reserve(value.SizeBytes());
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Base::Result<void> append =
            replacement.AppendUnchecked(
                Base::StringView(&lower, 1U));
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool RegistrationMatches(
    const XamlProviderRegistration& registration,
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) noexcept {
    const bool schemeMatches = requireScheme
        ? !registration.scheme.Empty() &&
            registration.scheme.View() == uri.Scheme()
        : registration.scheme.Empty();
    const bool assemblyMatches = requireAssembly
        ? !registration.assembly.Empty() &&
            registration.assembly.View() == uri.Assembly()
        : registration.assembly.Empty();
    return schemeMatches && assemblyMatches;
}

Base::Result<::Aero::Markup::StreamResourceInfo> CreateMemoryResource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    ::Aero::Markup::StreamResourceInfo result;
    result.uri = uri;
    result.stream = std::move(stream).Value();
    result.revision = revision;
    return result;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const Base::ResourceUri& baseUri) noexcept {
    // WPF component URIs beginning with '/' are assembly resources, not
    // filesystem-rooted paths. Resolving them against a file-backed App.xaml
    // would otherwise incorrectly manufacture a file: URI.
    if (uri.SizeBytes() > 0U && uri[0] == '/') {
        for (std::uint32_t index = 1U;
             index + 10U <= uri.SizeBytes(); ++index) {
            if (uri.Substr(index, 10U) ==
                Base::StringView(";component")) {
                return Base::ResourceUri::Parse(uri);
            }
        }
    }
    if (!baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}
} // namespace

Base::Result<void> XamlProviderRegistry::Set(
    Base::Ref<XamlProvider> provider,
    Base::StringView scheme,
    Base::StringView assembly,
    Base::Ref<XamlProvider>* replaced) noexcept {
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Markup XAML provider is required");
    }
    XamlProviderRegistration registration;
    Base::Result<void> schemeResult =
        AssignLowerAscii(registration.scheme, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Base::Result<void> assemblyResult =
        registration.assembly.Assign(assembly);
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    registration.provider = std::move(provider);
    for (const XamlProviderRegistration& existing : registrations_) {
        if (existing.provider.Get() == registration.provider.Get()) {
            registration.identity = existing.identity;
            break;
        }
    }
    static std::atomic<std::uint64_t> nextIdentity{1U};
    if (registration.identity == 0U) {
        registration.identity =
            nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        if (registration.identity == 0U) {
            registration.identity =
                nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    for (XamlProviderRegistration& existing : registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            if (replaced != nullptr) {
                *replaced = std::move(existing.provider);
            }
            existing = std::move(registration);
            return {};
        }
    }
    registrations_.PushBack(
        std::move(registration));
    return {};
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveRoute(
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (!RegistrationMatches(
                registration, uri, requireScheme, requireAssembly)) {
            continue;
        }
        XamlProviderResolution result;
        result.provider = registration.provider.Get();
        result.cacheIdentity = Base::MixHash64(
            registration.identity ^
            Base::DefaultHash<Base::StringView>{}(
                registration.scheme.View()) ^
            Base::DefaultHash<Base::StringView>{}(
                registration.assembly.View(), UINT64_C(0xA3E0)));
        return result;
    }
    if (parent_ != nullptr) {
        Base::Result<XamlProviderResolution> inherited =
            parent_->ResolveRoute(uri, requireScheme, requireAssembly);
        if (inherited) return inherited;
        if (inherited.GetStatus().code != Base::ErrorCode::NotFound) {
            return inherited.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches this route shape");
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveDetailed(
    const Base::ResourceUri& uri) const noexcept {
    const struct Route {
        bool scheme;
        bool assembly;
    } routes[] = {
        {true, true},
        {true, false},
        {false, true},
        {false, false}};

    for (const Route route : routes) {
        if ((route.scheme && uri.Scheme().Empty()) ||
            (route.assembly && uri.Assembly().Empty())) {
            continue;
        }
        Base::Result<XamlProviderResolution> resolved =
            ResolveRoute(uri, route.scheme, route.assembly);
        if (resolved) return resolved;
        if (resolved.GetStatus().code != Base::ErrorCode::NotFound) {
            return resolved.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches the resource URI");
}

Base::Result<XamlProvider*>
XamlProviderRegistry::Resolve(
    const Base::ResourceUri& uri) const noexcept {
    Base::Result<XamlProviderResolution> resolved =
        ResolveDetailed(uri);
    return resolved
        ? Base::Result<XamlProvider*>(resolved.Value().provider)
        : Base::Result<XamlProvider*>(resolved.GetStatus());
}

bool XamlProviderRegistry::Contains(
    const XamlProvider& provider) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (registration.provider.Get() == &provider) return true;
    }
    return false;
}

Base::Result<void> EmbeddedXamlProvider::Add(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Embedded XAML source provider is frozen");
    }
    if (uri.Empty() || bytes.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Embedded XAML source registration is invalid");
    }
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Embedded XAML source URI is already registered");
        }
    }

    Entry entry;
    entry.uri = uri;
    entry.bytes.Append(bytes);
    entry.revision = revision;
    entries_.PushBack(std::move(entry));
    return {};
}

Base::Result<void> EmbeddedXamlProvider::AddText(
    const Base::ResourceUri& uri,
    Base::StringView text,
    std::uint64_t revision) noexcept {
    return Add(
        uri,
        Base::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.Data()),
            text.SizeBytes()),
        revision);
}

Base::Result<void> EmbeddedXamlProvider::Freeze() noexcept {
    frozen_ = true;
    return {};
}

Base::Result<::Aero::Markup::StreamResourceInfo>
EmbeddedXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return CreateMemoryResource(
                entry.uri,
                entry.bytes.AsSpan(),
                entry.revision);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> EmbeddedXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) return entry.revision;
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> FileXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() || maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }
    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) return assigned.GetStatus();
    std::error_code error;
    const std::filesystem::path filePath(path.CStr());
    const std::uintmax_t size =
        std::filesystem::file_size(filePath, error);
    if (error || size > maxFileBytes_ || size > UINT32_MAX) {
        return Base::Status::Failure(
            error ? Base::ErrorCode::NotFound : Base::ErrorCode::OutOfRange,
            "XAML source file revision could not be read");
    }
    const auto writeTime =
        std::filesystem::last_write_time(filePath, error);
    if (error) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file timestamp could not be read");
    }
    const std::uint64_t ticks = static_cast<std::uint64_t>(
        writeTime.time_since_epoch().count());
    return Base::MixHash64(
        static_cast<std::uint64_t>(size) ^ Base::MixHash64(ticks));
}

Base::Result<::Aero::Markup::StreamResourceInfo>
FileXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() ||
        maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }

    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
    if (!assigned) {
        return assigned.GetStatus();
    }
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.CStr(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.CStr(), "rb");
#endif
    if (file == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file could not be opened");
    }
    if (std::fseek(file, 0L, SEEK_END) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file size could not be read");
    }
    const long length = std::ftell(file);
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > maxFileBytes_ ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML source file exceeds configured limits");
    }
    if (std::fseek(file, 0L, SEEK_SET) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file could not be rewound");
    }

    Base::Result<Base::Ref<FileStream>> stream =
        Base::MakeRef<FileStream>(
            file, static_cast<std::uint64_t>(length));
    if (!stream) {
        std::fclose(file);
        return stream.GetStatus();
    }
    ::Aero::Markup::StreamResourceInfo source;
    source.uri = uri;
    source.stream = std::move(stream).Value();
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : 0U;
    return source;
}

struct Loader::LoaderState::Operation {
    struct FinalizeState {
        Operation* operation = nullptr;
        const XamlReaderSettings* options = nullptr;
        const Base::ResourceUri* origin = nullptr;
        const CompiledDocument* compiled = nullptr;
    };

    struct PendingResourceMerge {
        ResourceDictionary target;
        ResourceDictionary source;
    };

    Operation(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics,
        const LoadState* loadState) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          loadState_(loadState) {}

    const LoadState& LoadContext() const noexcept {
        static const LoadState empty;
        return loadState_ != nullptr ? *loadState_ : empty;
    }

    Base::Result<LoaderResult> LoadCore(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<LoaderResult> ParseCore(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false) noexcept;
    Base::Result<LoaderResult> ParseStreamCore(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false,
        CompiledDocument* outCompiled = nullptr) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options) noexcept;
    Base::Result<LoaderResult> LoadCompiledDocument(
        const CompiledDocument& document,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<void> ResolveResourceDependencies(
        LoaderResult& result,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> ResolveDictionaryDependencies(
        ResourceDictionary& dictionary,
        LoaderResult& owner,
        const XamlReaderSettings& options,
        std::uint32_t& resourceCount,
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> CommitResourceDependencies(
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> AppendDependencies(
        LoaderResult& destination,
        const LoaderResult& source,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> AppendDependency(
        LoaderResult& destination,
        const Base::ResourceUri& dependency,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> FinalizeResult(
        LoaderResult& result,
        const XamlReaderSettings& options,
        const Base::ResourceUri& origin,
        const CompiledDocument* compiled) noexcept;
    static Base::Result<void> FinalizeLoad(
        LoaderResult& result,
        void* context) noexcept;
    Base::Result<void> ValidateOptions(
        const XamlReaderSettings& options) const noexcept;
    Base::Result<void> CheckPolicy(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options) noexcept;
    bool IsLoading(const Base::ResourceUri& uri) const noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode code,
        Base::StringView message) noexcept;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* loadState_ = nullptr;
    Base::Vector<Base::ResourceUri> loadStack_;
};

Base::Result<LoaderResult>
Loader::LoaderState::Operation::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (bytes.Size() > options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Compiled XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Compiled XAML source exceeds configured limits"));
    }
    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            bytes,
            schema_->Domain(),
            options.limits.compiled);
    if (!document) {
        const Base::Status status = document.GetStatus();
        if (!originUri.Empty() &&
            (status.code == Base::ErrorCode::Unsupported ||
             status.code == Base::ErrorCode::ValidationFailed)) {
            // A compatible source is authoritative when the cache identity no
            // longer matches this runtime. Hosts may persist a replacement
            // cache after this successful source load.
            return LoadCore(originUri, options, {});
        }
        return status;
    }

    return LoadCompiledDocument(
        document.Value(), originUri, options, {});
}

Base::Result<LoaderResult>
Loader::LoaderState::Operation::LoadCompiledDocument(
    const CompiledDocument& document,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& loadState = LoadContext();
    LoadState context;
    context.resources = loadState.resources;
    context.effectiveValues = loadState.effectiveValues;
    context.bindings = loadState.bindings;
    context.fallbackResources = loadState.fallbackResources;
    context.baseUri = &originUri;
    context.templatedParent = loadState.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = loadState.effectLifetime;
    context.effectCommitMode = loadState.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    // Source-backed compiled documents need the same two-phase resource
    // resolution as streamed documents. Merged ResourceDictionary sources
    // are committed by the load finalizer before queued StaticResource
    // references are written.
    context.deferUnresolvedStaticResources = true;
    FinalizeState finalize{
        this, &options, &originUri, &document};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        loadState.dispatcher != nullptr &&
        loadState.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *loadState.dispatcher,
                  *loadState.dependencyProperties,
                  schema_->Metadata());
              return writer.Load(document, context);
          }()
        : [&]() noexcept {
              return writer.Load(document, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::LoadCore(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& loadState = LoadContext();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    Base::Result<void> policy = CheckPolicy(uri, options);
    if (!policy) {
        return policy.GetStatus();
    }
    if (IsLoading(uri)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive XAML source load was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive XAML source load was detected"));
    }
    if (loadStack_.Size() >=
        options.limits.maxDependencyDepth) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency depth exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency depth exceeds configured limits"));
    }

    Base::Result<XamlProviderResolution> provider =
        providers_->ResolveDetailed(uri);
    if (!provider) {
        return Failure(
            provider.GetStatus(),
            LoaderDiagnosticCodes::XamlProviderNotFound,
            Base::StringView(
                "No XAML source provider matches the resource URI"));
    }
    loadStack_.PushBack(uri);

    if (loadState.documentCache != nullptr) {
        Base::Result<std::uint64_t> probedRevision =
            provider.Value().provider->Revision(uri);
        if (probedRevision && probedRevision.Value() != 0U) {
            Base::Result<DocumentCacheLookup> cached =
                loadState.documentCache->Lookup(
                    uri,
                    probedRevision.Value(),
                    provider.Value().cacheIdentity,
                    schema_->Domain(),
                    options.limits.compiled);
            if (cached && cached.Value().hit && cached.Value().document != nullptr) {
                Base::Result<LoaderResult> loaded =
                    LoadCompiledDocument(
                        *cached.Value().document,
                        uri,
                        options,
                        existingRoot);
                loadStack_.PopBack();
                return loaded;
            }
        }
    }

    Base::Result<::Aero::Markup::StreamResourceInfo> source =
        provider.Value().provider->Open(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            LoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    ::Aero::Markup::StreamResourceInfo sourceInfo =
        std::move(source).Value();
    if (!sourceInfo.stream ||
        !sourceInfo.stream->CanRead()) {
        loadStack_.PopBack();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source stream is invalid"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source stream could not be read"));
    }

    const Base::ResourceUri& origin =
        sourceInfo.uri.Empty()
        ? uri
        : sourceInfo.uri;
    HashingStream hashing(*sourceInfo.stream);
    CompiledDocument compiledDoc;
    Base::Result<LoaderResult> loaded = ParseStreamCore(
        hashing,
        origin,
        options,
        existingRoot,
        true,
        loadState.documentCache != nullptr
            ? &compiledDoc
            : nullptr);
    if (sourceInfo.revision == 0U) {
        sourceInfo.revision = hashing.Hash();
    }
    if (loaded && loadState.documentCache != nullptr &&
        compiledDoc.IsValid()) {
        bool dependenciesOk = true;
        for (const Base::ResourceUri& dependency :
             loaded.Value().dependencies) {
            Base::Result<void> added =
                compiledDoc.AddDependency(dependency);
            if (!added) {
                dependenciesOk = false;
                break;
            }
        }
        if (dependenciesOk) {
            static_cast<void>(loadState.documentCache->Store(
                uri,
                sourceInfo.revision,
                provider.Value().cacheIdentity,
                std::move(compiledDoc),
                {loaded.Value().dependencies.Data(),
                 loaded.Value().dependencies.Size()}));
        }
    }
    loadStack_.PopBack();
    return loaded;
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::ParseCore(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (text.SizeBytes() >
        options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML source exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

    const Base::Span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(text.Data()),
        text.SizeBytes()};
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    return ParseStreamCore(
        *stream.Value(),
        baseUri,
        options,
        existingRoot,
        deferUnresolvedStaticResources);
}

Base::Result<LoaderResult> Loader::LoaderState::Operation::ParseStreamCore(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    [[maybe_unused]] bool deferUnresolvedStaticResources,
    CompiledDocument* outCompiled) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }

    Utf8XmlTokenizer tokenizer(options.limits.xml);
    Base::Result<void> reset =
        tokenizer.Reset(stream, diagnostics_);
    if (!reset) return reset.GetStatus();

    NodeReader reader(tokenizer, diagnostics_);
    Base::Result<CompiledDocument> compiledDoc =
        CompiledDocument::Compile(reader, *schema_, baseUri);
    if (!compiledDoc) return compiledDoc.GetStatus();

    Base::Result<LoaderResult> lr = LoadCompiledDocument(
        compiledDoc.Value(), baseUri, options, existingRoot);
    if (lr && outCompiled != nullptr) {
        *outCompiled = std::move(compiledDoc).Value();
    }
    return lr;
}

Base::Result<void> Loader::LoaderState::Operation::FinalizeLoad(
    LoaderResult& result,
    void* context) noexcept {
    auto* finalize =
        static_cast<FinalizeState*>(context);
    if (finalize == nullptr ||
        finalize->operation == nullptr ||
        finalize->options == nullptr ||
        finalize->origin == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML load finalization context is invalid");
    }
    return finalize->operation->FinalizeResult(
        result,
        *finalize->options,
        *finalize->origin,
        finalize->compiled);
}

Base::Result<void> Loader::LoaderState::Operation::FinalizeResult(
    LoaderResult& result,
    const XamlReaderSettings& options,
    const Base::ResourceUri& origin,
    const CompiledDocument* compiled) noexcept {
    const Base::ResourceUri& effectiveOrigin =
        compiled != nullptr && origin.Empty()
        ? compiled->OriginUri()
        : origin;
    result.canonicalUri = effectiveOrigin;
    if (compiled != nullptr) {
        for (const Base::ResourceUri& dependency :
             compiled->Dependencies()) {
            Base::Result<void> appended =
                AppendDependency(
                    result, dependency, options);
            if (!appended) return appended.GetStatus();
        }
    }
    Base::Result<void> originDependency =
        AppendDependency(
            result, effectiveOrigin, options);
    if (!originDependency) {
        return originDependency.GetStatus();
    }
    return ResolveResourceDependencies(
        result, options);
}

Base::Result<void>
Loader::LoaderState::Operation::ResolveResourceDependencies(
    LoaderResult& result,
    const XamlReaderSettings& options) noexcept {
    std::uint32_t resourceCount = 0U;
    Base::Vector<PendingResourceMerge> pending;

    auto resolveDictionary = [&](ResourceDictionary& dictionary)
        noexcept -> Base::Result<void> {
        if (dictionary.Size() == 0U &&
            dictionary.MergedDictionaryCount() == 0U &&
            dictionary.GetSource().Empty()) {
            return {};
        }
        return ResolveDictionaryDependencies(
            dictionary,
            result,
            options,
            resourceCount,
            pending);
    };

    Base::Result<void> resolved = resolveDictionary(result.resources);
    if (!resolved) return resolved.GetStatus();

    ResourceDictionary* rootResources = nullptr;
    if (result.root) {
        rootResources = schema_->ResolveResourceScope(result.root->RuntimeType(), *result.root);
        if (rootResources != nullptr) {
            resolved = resolveDictionary(*rootResources);
            if (!resolved) return resolved.GetStatus();
        }
    }

    for (Aero::Media::Visual* visual : result.visualContent.nodes) {
        if (visual == nullptr ||
            (result.root && visual == result.root.Get())) {
            continue;
        }
        ResourceDictionary* resources = schema_->ResolveResourceScope(visual->RuntimeType(), *visual);
        if (resources == nullptr || resources == rootResources) continue;
        resolved = resolveDictionary(*resources);
        if (!resolved) return resolved.GetStatus();
    }
    return CommitResourceDependencies(pending);
}

Base::Result<void>
Loader::LoaderState::Operation::ResolveDictionaryDependencies(
    ResourceDictionary& dictionary,
    LoaderResult& owner,
    const XamlReaderSettings& options,
    std::uint32_t& resourceCount,
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    if (resourceCount >
            options.limits.maxResources ||
        dictionary.Size() >
            options.limits.maxResources - resourceCount) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML resource count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML resource count exceeds configured limits"));
    }
    resourceCount += dictionary.Size();

    for (std::uint32_t index = 0U;
         index < dictionary.Size();
         ++index) {
        Base::Result<Aero::ResourceEntrySnapshot>
            entry = dictionary.EntryAt(index);
        if (!entry) return entry.GetStatus();
        const Meta::Value& value = entry.Value().value;
        if (value.Kind() != Meta::ValueKind::Object ||
            value.IsNullObject() ||
            !value.AsObject()) {
            continue;
        }
        Base::Object& object = *value.AsObject();
        ResourceDictionary* nested = schema_->ResolveResourceScope(object.RuntimeType(), object);
        if (nested == nullptr ||
            (nested->Size() == 0U &&
             nested->MergedDictionaryCount() == 0U &&
             nested->GetSource().Empty())) {
            continue;
        }
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                *nested,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const std::uint32_t mergedCount =
        dictionary.MergedDictionaryCount();
    for (std::uint32_t index = 0U;
         index < mergedCount;
         ++index) {
        ResourceDictionary merged =
            dictionary.MergedDictionaryAt(index);
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                merged,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const Base::ResourceUri source =
        dictionary.GetSource();
    if (source.Empty()) return {};
    Base::ResourceUri effectiveSource = source;
    if (!source.IsAbsolute() && !owner.canonicalUri.Empty()) {
        Base::Result<Base::ResourceUri> resolved =
            ResolveRequestedUri(source.Canonical(), owner.canonicalUri);
        if (resolved) {
            effectiveSource = std::move(resolved).Value();
        }
    }
    if ((!owner.canonicalUri.Empty() &&
         effectiveSource == owner.canonicalUri) ||
        IsLoading(effectiveSource)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive ResourceDictionary Source was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive ResourceDictionary Source was detected"));
    }

    // Merged dictionaries are evaluated in declaration order. Supply already
    // discovered siblings as ambient resources while loading the next source,
    // so WPF-style DynamicResource values in styles can resolve against an
    // earlier palette or brush dictionary.
    ResourceDictionary ambientResources;
    // Already-loaded sibling Source dictionaries first so a later merge
    // (Resources.xaml) can StaticResource keys defined in nested merges of
    // an earlier sibling (Colors.Dark inside Brushes.DarkBlue).
    for (PendingResourceMerge& discovered : pending) {
        ambientResources.AddMerged(discovered.source);
    }
    if (dictionary.Size() > 0U ||
        dictionary.MergedDictionaryCount() > 0U) {
        ambientResources.AddMerged(dictionary);
    }
    const LoadState& loadState = LoadContext();
    LoadState resourceContext = loadState;
    resourceContext.resources = &ambientResources;
    resourceContext.fallbackResources = &ambientResources;
    const LoadState* previousLoadState = loadState_;
    loadState_ = &resourceContext;
    Base::Result<LoaderResult> loaded =
        LoadCore(effectiveSource, options, {});
    loadState_ = previousLoadState;
    if (!loaded) {
        return Failure(
            loaded.GetStatus(),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source could not be loaded"));
    }
    if (!loaded.Value().root ||
        loaded.Value().root->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        loaded.Value().Clear();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "ResourceDictionary Source root has an incompatible type"),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source root must be ResourceDictionary"));
    }
    auto& sourceDictionary =
        static_cast<ResourceDictionary&>(
            *loaded.Value().root);
    Base::Result<void> dependencies =
        AppendDependencies(owner, loaded.Value(), options);
    if (!dependencies) {
        loaded.Value().Clear();
        return dependencies.GetStatus();
    }
    PendingResourceMerge merge;
    Base::Result<ResourceDictionary> target =
        dictionary.Share();
    if (!target) {
        loaded.Value().Clear();
        return target.GetStatus();
    }
    merge.target = std::move(target).Value();
    merge.source = std::move(sourceDictionary);
    pending.PushBack(std::move(merge));
    return {};
}

Base::Result<void>
Loader::LoaderState::Operation::CommitResourceDependencies(
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    for (PendingResourceMerge& merge : pending) {
        merge.target.AddMerged(merge.source);
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::AppendDependencies(
    LoaderResult& destination,
    const LoaderResult& source,
    const XamlReaderSettings& options) noexcept {
    for (const Base::ResourceUri& dependency :
         source.dependencies) {
        Base::Result<void> appended =
            AppendDependency(
                destination, dependency, options);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::AppendDependency(
    LoaderResult& destination,
    const Base::ResourceUri& dependency,
    const XamlReaderSettings& options) noexcept {
    if (dependency.Empty()) return {};
    for (const Base::ResourceUri& existing :
         destination.dependencies) {
        if (existing == dependency) return {};
    }
    if (destination.dependencies.Size() >=
        options.limits.compiled.maxDependencies) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency count exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency count exceeds configured limits"));
    }
    destination.dependencies.PushBack(
        dependency);
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::ValidateOptions(
    const XamlReaderSettings& options) const noexcept {
    const LoadState& loadState = LoadContext();
    if (schema_ == nullptr || providers_ == nullptr ||
        !schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML loader requires a frozen schema and provider registry");
    }
    if (options.limits.maxSourceBytes == 0U ||
        options.limits.maxObjects == 0U ||
        options.limits.maxResources == 0U ||
        options.limits.maxDependencyDepth == 0U ||
        options.limits.xml.maxInputBytes == 0U ||
        options.limits.xml.maxDepth == 0U ||
        options.limits.compiled.maxNodes == 0U ||
        options.limits.compiled.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML load limits must be positive");
    }
    if ((loadState.dispatcher == nullptr) !=
        (loadState.dependencyProperties == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML object factory require dispatcher and property metadata");
    }
    return {};
}

Base::Result<void> Loader::LoaderState::Operation::CheckPolicy(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (uri.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML resource URI cannot be empty"),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI cannot be empty"));
    }
    if (uri.IsNetwork() &&
        !options.policy.allowNetwork) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Network XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Network XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("file") &&
        !options.policy.allowFile) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "File XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "File XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("pack") &&
        !options.policy.allowPackApplication) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Pack application XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Pack application XAML sources are disabled by policy"));
    }
    return {};
}

bool Loader::LoaderState::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status Loader::LoaderState::Operation::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode code,
    Base::StringView message) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> diagnostic =
            ::Aero::Diagnostics::Diagnostic::Create(
                code,
                ::Aero::Diagnostics::DiagnosticSeverity::Error,
                message);
        if (diagnostic) {
            if (status.message != nullptr &&
                status.message[0] != '\0') {
                const Base::StringView detail(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message)));
                if (detail != message) {
                    diagnostic.Value().AddNote(detail);
                }
            }
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    return status;
}
} // namespace Aero::Markup

namespace Aero::Markup {
namespace {
Base::Result<XamlDocument> AdoptResult(
    Base::Result<LoaderResult>&& loaded,
    Base::IAllocator& allocator) noexcept {
    if (!loaded) return loaded.GetStatus();
    Base::Result<XamlDocument> document =
        ::Aero::Markup::AdoptXamlDocument(
            std::move(loaded).Value(), allocator);
    return document;
}
} // namespace

Loader::Loader(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    Base::IAllocator* allocator,
    const LoadState* loadState) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_,
        schema, providers, diagnostics, loadState));
}

Loader::~Loader() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

Base::Result<XamlDocument> Loader::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return AdoptResult(
            operation.Failure(
                resolved.GetStatus(),
                LoaderDiagnosticCodes::InvalidUri,
                Base::StringView("XAML resource URI is invalid")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(resolved.Value(), options, {}),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.LoadCore(uri, options, {}), *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.ParseCore(text, baseUri, options, {}, true),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.ParseStreamCore(
            stream, baseUri, options, {}, true),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return AdoptResult(
            operation.Failure(
                resolved.GetStatus(),
                LoaderDiagnosticCodes::InvalidUri,
                Base::StringView("XAML component URI is invalid")),
            *allocator_);
    }
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return AdoptResult(
            operation.Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "LoadComponent requires a managed root object"),
                LoaderDiagnosticCodes::LoadComponentTypeMismatch,
                Base::StringView(
                    "XAML component root cannot be retained")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(resolved.Value(), options, retained),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return AdoptResult(
            operation.Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "LoadComponent requires a managed root object"),
                LoaderDiagnosticCodes::LoadComponentTypeMismatch,
                Base::StringView(
                    "XAML component root cannot be retained")),
            *allocator_);
    }
    return AdoptResult(
        operation.LoadCore(uri, options, retained), *allocator_);
}
Base::Result<XamlDocument> Loader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    LoaderState::Operation operation(
        *state_->schema_, *state_->providers_,
        state_->diagnostics_, state_->loadState_);
    return AdoptResult(
        operation.LoadCompiled(bytes, originUri, options),
        *allocator_);
}
} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlReader / XamlLoader / DocumentCache / Coordinator Implementation
// ============================================================================

// ============================================================================
// SECTION: XamlLoader / CompiledCache / DocumentState
// ============================================================================
// Consolidated implementation. Keep sections ordered by dependency.

// ===== CompiledCache =====

namespace Aero::Markup {
Base::Result<CompiledCacheIdentity> BuildCompiledCacheIdentity(
    const ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<Base::HashCode> hash = domain.ComputeSchemaHash();
    if (!hash) return hash.GetStatus();

    CompiledCacheIdentity identity;
    identity.metadataSchemaHash = hash.Value();
    return identity;
}

CompiledCacheCompatibility CompareCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const CompiledCacheIdentity& current) noexcept {
    if (cached.cacheFormatVersion != current.cacheFormatVersion) {
        return CompiledCacheCompatibility::CacheFormatMismatch;
    }
    if (cached.typeIdAlgorithmVersion != current.typeIdAlgorithmVersion) {
        return CompiledCacheCompatibility::TypeIdAlgorithmMismatch;
    }
    if (cached.metadataSchemaFormatVersion !=
        current.metadataSchemaFormatVersion) {
        return CompiledCacheCompatibility::MetadataSchemaFormatMismatch;
    }
    if (cached.metadataProgramFormatVersion !=
        current.metadataProgramFormatVersion) {
        return CompiledCacheCompatibility::MetadataProgramFormatMismatch;
    }
    if (cached.schemaVersion != current.schemaVersion) {
        return CompiledCacheCompatibility::SchemaVersionMismatch;
    }
    if (cached.metadataSchemaHash != current.metadataSchemaHash) {
        return CompiledCacheCompatibility::MetadataSchemaMismatch;
    }
    return CompiledCacheCompatibility::Compatible;
}

Base::Result<void> ValidateCompiledCacheIdentity(
    const CompiledCacheIdentity& cached,
    const ::Aero::Meta::Registry& currentDomain) noexcept {
    Base::Result<CompiledCacheIdentity> current =
        BuildCompiledCacheIdentity(currentDomain);
    if (!current) return current.GetStatus();

    switch (CompareCompiledCacheIdentity(cached, current.Value())) {
    case CompiledCacheCompatibility::Compatible:
        return {};
    case CompiledCacheCompatibility::CacheFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML cache format version is incompatible");
    case CompiledCacheCompatibility::TypeIdAlgorithmMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML TypeId algorithm version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata descriptor format is incompatible");
    case CompiledCacheCompatibility::MetadataProgramFormatMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML metadata facet format is incompatible");
    case CompiledCacheCompatibility::SchemaVersionMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Compiled XAML schema ABI version is incompatible");
    case CompiledCacheCompatibility::MetadataSchemaMismatch:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML metadata schema hash does not match the runtime");
    }
    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        "Compiled XAML compatibility result is invalid");
}
} // namespace Aero::Markup

// ===== Resources =====

namespace Aero::Markup {
namespace {
using namespace Aero::Meta;
using namespace Aero::Threading;

Base::Status InvalidResource(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Base::Result<void> AddResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    if (scopeOwner.RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "XAML resource scope is not a ResourceDictionary");
    }
    static_cast<ResourceDictionary&>(scopeOwner)
        .Add(key, value);
    return {};
}

Base::Result<void> AddFrameworkResource(
    Base::Object& scopeOwner,
    const ResourceKey& key,
    const Meta::Value& value,
    void*) noexcept {
    auto* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&scopeOwner);
    if (element == nullptr) {
        return InvalidResource(
            "XAML resource scope is not a FrameworkElement");
    }
    element->GetResources().Add(key, value);
    return {};
}

ResourceDictionary* ResolveDictionaryScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    return scopeOwner.RuntimeType() ==
            ResourceDictionary::StaticTypeId()
        ? &static_cast<ResourceDictionary&>(
              scopeOwner)
        : nullptr;
}

ResourceDictionary* ResolveFrameworkScope(
    Base::Object& scopeOwner,
    void*) noexcept {
    ::Aero::Media::Visual& visual =
        static_cast<::Aero::Media::Visual&>(scopeOwner);
    FrameworkElement* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&(visual));
    return element != nullptr
        ? &element->GetResources()
        : nullptr;
}

Base::Result<void> RegisterFrameworkName(
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object,
    void*) noexcept {
    FrameworkElement* element =
        ::Aero::TryCast<::Aero::FrameworkElement>(&scopeOwner);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML name scope is not a FrameworkElement");
    }
    element->RegisterName(name, object);
    return {};
}
} // namespace

Base::Result<void> ResourceExtension::Register(
    Schema& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML resource extension registration is invalid");
    }
    const PropertyInfo* source =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Source"),
            false);
    const PropertyInfo* merged =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("MergedDictionaries"),
            false);
    const PropertyInfo* entries =
        schema.Types().FindProperty(
            ResourceDictionary::StaticTypeId(),
            Base::StringView("Entries"),
            false);
    if (source == nullptr || merged == nullptr ||
        entries == nullptr ||
        source->ValueType() !=
            TypeOf<Base::ResourceUri>() ||
        merged->ValueType() !=
            ResourceDictionary::StaticTypeId()) {
        return InvalidResource(
            "ResourceDictionary XAML metadata is incomplete");
    }

    schema_ = &schema;
    Base::Result<void> status =
        schema.AddResourceScope({
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddResourceScope({
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddNameScope({
        FrameworkElement::StaticTypeId(),
        false,
        &RegisterFrameworkName,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = schema.AddNameScope({
        ::Aero::Controls::UserControl::StaticTypeId(),
        true,
        &RegisterFrameworkName,
        nullptr});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}
} // namespace Aero::Markup

// ===== XamlDocument =====

namespace Aero::Markup {
struct XamlDocumentState {
    explicit XamlDocumentState(LoaderResult&& value) noexcept
        : result(std::move(value)) {}

    static Base::Result<XamlDocument> Adopt(
        LoaderResult&& result,
        Base::IAllocator& allocator) noexcept;
    static LoaderResult Take(
        XamlDocument& document) noexcept;
    static const EffectLifetime* RuntimeLifetime(
        const XamlDocument& document) noexcept;

    LoaderResult result;
};

XamlDocument::~XamlDocument() noexcept {
    Reset();
}

XamlDocument::XamlDocument(XamlDocument&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

XamlDocument& XamlDocument::operator=(XamlDocument&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}
} // namespace Aero::Markup

namespace Aero::Markup {
Base::Result<::Aero::Markup::XamlDocument> AdoptXamlDocument(
    ::Aero::Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    return ::Aero::Markup::XamlDocumentState::Adopt(
        std::move(result), allocator);
}
} // namespace Aero::Markup

namespace Aero::Markup {
Base::Result<XamlDocument> XamlDocumentState::Adopt(
    LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    XamlDocument document;
    document.allocator_ = &allocator;
    document.state_ =
        new (memory) XamlDocumentState(std::move(result));
    return document;
}
} // namespace Aero::Markup

namespace Aero::Markup {
bool XamlDocument::IsValid() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr && state->result.root;
}

const Base::Ref<Base::Object>& XamlDocument::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.root : empty;
}

Base::Object* XamlDocument::RootObject(
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || !state->result.root) return nullptr;
    Base::Object* root = state->result.root.Get();
    if (expectedType == Meta::InvalidTypeId) return root;
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        root->RuntimeType(), expectedType)
        ? root
        : nullptr;
}

Base::Object* XamlDocument::FindName(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr || name.Empty()) return nullptr;
    Base::Object* object = state->result.names.Find(name);
    if (object == nullptr || expectedType == Meta::InvalidTypeId) {
        return object;
    }
    const Meta::Registry* metadata = state->result.metadata;
    return metadata != nullptr && metadata->Types().IsDerivedFrom(
        object->RuntimeType(), expectedType)
        ? object
        : nullptr;
}

std::uint32_t XamlDocument::NamedObjectCount() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.names.Size() : 0U;
}

Aero::ResourceDictionary* XamlDocument::Resources() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Aero::ResourceDictionary* XamlDocument::Resources() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? &state->result.resources : nullptr;
}

const Base::ResourceUri& XamlDocument::CanonicalUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr ? state->result.canonicalUri : empty;
}

Base::Span<const Base::ResourceUri> XamlDocument::Dependencies() const noexcept {
    const auto* state = static_cast<const XamlDocumentState*>(state_);
    return state != nullptr
        ? Base::Span<const Base::ResourceUri>{
              state->result.dependencies.Data(),
              state->result.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}
} // namespace Aero::Markup

namespace Aero::Markup {
const EffectLifetime*
XamlDocumentState::RuntimeLifetime(
    const XamlDocument& document) noexcept {
    const auto* state =
        static_cast<const XamlDocumentState*>(document.state_);
    return state != nullptr
        ? state->result.runtimeLifetime.Get()
        : nullptr;
}

LoaderResult XamlDocumentState::Take(
    XamlDocument& document) noexcept {
    auto* state = static_cast<XamlDocumentState*>(document.state_);
    if (state == nullptr) {
        LoaderResult empty;
        return empty;
    }
    LoaderResult result = std::move(state->result);
    document.Reset();
    return result;
}
} // namespace Aero::Markup

namespace Aero::Markup {
const ::Aero::Markup::EffectLifetime*
XamlDocumentEffectLifetime(
    const ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::RuntimeLifetime(document);
}

::Aero::Markup::LoaderResult TakeXamlDocument(
    ::Aero::Markup::XamlDocument& document) noexcept {
    return ::Aero::Markup::XamlDocumentState::Take(document);
}
} // namespace Aero::Markup

namespace Aero::Markup {
void XamlDocument::Reset() noexcept {
    auto* state = static_cast<XamlDocumentState*>(state_);
    if (state == nullptr) return;
    state->result.Clear();
    state->~XamlDocumentState();
    allocator_->Deallocate(
        state,
        sizeof(XamlDocumentState),
        alignof(XamlDocumentState),
        Base::MemoryTag::Markup);
    state_ = nullptr;
    allocator_ = nullptr;
}
} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlProvider / Adapter
// ============================================================================

namespace Aero::Markup {
void XamlProvider::AddChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    changed_.Add(handler);
}

bool XamlProvider::RemoveChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    return changed_.Remove(handler);
}

void XamlProvider::RaiseChanged(
    const Base::ResourceUri& uri) noexcept {
    if (changed_) changed_(uri);
}

Base::Result<StreamResourceInfo>
XamlProviderAdapter::Open(
    const Base::ResourceUri& uri) const noexcept {
    if (open_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML provider has no open callback");
    }
    return open_(uri, context_);
}

Base::Result<std::uint64_t>
XamlProviderAdapter::Revision(
    const Base::ResourceUri& uri) const noexcept {
    return revision_ != nullptr
        ? revision_(uri, context_)
        : XamlProvider::Revision(uri);
}
} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlDocumentCache / DependencyGraph
// ============================================================================

// ===== DocumentCache =====

namespace Aero::Markup {
namespace {
Base::Result<Base::String> MakeKey(
    const Base::ResourceUri& uri,
    Base::IAllocator& allocator) noexcept {
    if (uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML cache URI cannot be empty");
    }
    Base::String key(&allocator);
    Base::Result<void> assigned = key.Assign(uri.Canonical());
    if (!assigned) return assigned.GetStatus();
    return key;
}

bool ContainsKey(
    const Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (const Base::String& value : values) {
        if (value.View() == key) return true;
    }
    return false;
}

void RemoveKey(
    Base::Vector<Base::String>& values,
    Base::StringView key) noexcept {
    for (std::uint32_t index = 0U; index < values.Size(); ++index) {
        if (values[index].View() != key) continue;
        if (index + 1U != values.Size()) {
            values[index] = std::move(values.Back());
        }
        values.PopBack();
        return;
    }
}
} // namespace

struct DependencyGraph::State {
    struct Node {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit State(Base::IAllocator& allocator) noexcept
        : allocator(&allocator), nodes(&allocator) {}

    Base::Result<Node*> EnsureNode(
        const Base::ResourceUri& uri) noexcept {
        Base::Result<Base::String> key =
            MakeKey(uri, *allocator);
        if (!key) return key.GetStatus();
        Node* current = nodes.Find(key.Value());
        if (current != nullptr) return current;
        Node node(*allocator);
        node.uri = uri;
        typename Base::HashMap<Base::String, Node>::InsertResult
            inserted = nodes.Insert(
                std::move(key).Value(), std::move(node));
        return &inserted.entry->Value();
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Node> nodes;
    std::uint64_t generation = 0U;
};

static_assert(
    sizeof(DependencyGraph::State) <= 2048,
    "DependencyGraph inline state storage is too small");
static_assert(
    alignof(DependencyGraph::State) <= alignof(std::max_align_t),
    "DependencyGraph inline state alignment is insufficient");

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_, *allocator_));
}

DependencyGraph::~DependencyGraph() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<void> DependencyGraph::Update(
    const Base::ResourceUri& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph is unavailable");
    }
    Base::Result<Base::String> documentKey =
        MakeKey(document, *allocator_);
    if (!documentKey) return documentKey.GetStatus();
    Base::Result<DependencyGraph::State::Node*> documentNode =
        state_->EnsureNode(document);
    if (!documentNode) return documentNode.GetStatus();

    Base::Vector<Base::String> newDependencies(allocator_);
    newDependencies.Reserve(dependencies.Size());
    for (const Base::ResourceUri& dependencyUri : dependencies) {
        if (dependencyUri.Empty() || dependencyUri == document) continue;
        Base::Result<Base::String> dependencyKey =
            MakeKey(dependencyUri, *allocator_);
        if (!dependencyKey) return dependencyKey.GetStatus();
        if (ContainsKey(newDependencies, dependencyKey.Value().View())) {
            continue;
        }
        Base::Result<DependencyGraph::State::Node*> dependencyNode =
            state_->EnsureNode(dependencyUri);
        if (!dependencyNode) return dependencyNode.GetStatus();
        newDependencies.PushBack(
            std::move(dependencyKey).Value());
    }

    // Prepare reverse-edge key ownership and vector capacity before mutating
    // any edge. No hash-map insertions occur after this point, so node
    // references remain stable even when EnsureNode() previously rehashed.
    Base::Vector<Base::String> reverseKeys(allocator_);
    reverseKeys.Reserve(newDependencies.Size());
    for (const Base::String& dependencyKey : newDependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML dependency graph lost a prepared node");
        }
        dependency->dependents.Reserve(
                dependency->dependents.Size() + 1U);
        Base::String reverseKey(allocator_);
        Base::Result<void> copied = reverseKey.Assign(
            documentKey.Value().View());
        if (!copied) return copied.GetStatus();
        reverseKeys.PushBack(
            std::move(reverseKey));
    }

    DependencyGraph::State::Node* node = state_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(oldDependency);
        if (dependency == nullptr) continue;
        RemoveKey(
            dependency->dependents,
            documentKey.Value().View());
        if (!ContainsKey(newDependencies, oldDependency.View()) &&
            dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(oldDependency);
        }
    }
    node->dependencies = std::move(newDependencies);
    for (std::uint32_t index = 0U;
         index < node->dependencies.Size();
         ++index) {
        DependencyGraph::State::Node* dependency =
            state_->nodes.Find(node->dependencies[index]);
        if (dependency == nullptr) continue;
        if (ContainsKey(
                dependency->dependents,
                documentKey.Value().View())) {
            continue;
        }
        dependency->dependents.PushBack(
                std::move(reverseKeys[index]));
    }
    ++state_->generation;
    return {};
}

bool DependencyGraph::Remove(
    const Base::ResourceUri& document) noexcept {
    if (state_ == nullptr || document.Empty()) return false;
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return false;
    DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    previousDependencies.Append(
            node->dependencies.AsSpan());
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        RemoveKey(dependency->dependents, key.Value().View());
        if (dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            state_->nodes.Erase(dependencyKey);
        }
    }
    if (node->dependents.Empty()) {
        state_->nodes.Erase(key.Value());
    }
    ++state_->generation;
    return true;
}

void DependencyGraph::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->nodes.Clear();
    ++state_->generation;
}

Base::Result<void> DependencyGraph::CopyDependencies(
    const Base::ResourceUri& document,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || document.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    output.Reserve(node->dependencies.Size());
    for (const Base::String& dependencyKey : node->dependencies) {
        const DependencyGraph::State::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        output.PushBack(dependency->uri);
    }
    return {};
}

Base::Result<void> DependencyGraph::CopyDependents(
    const Base::ResourceUri& dependency,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || dependency.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(dependency, *allocator_);
    if (!key) return key.GetStatus();
    const DependencyGraph::State::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    output.Reserve(node->dependents.Size());
    for (const Base::String& dependentKey : node->dependents) {
        const DependencyGraph::State::Node* dependent = state_->nodes.Find(dependentKey);
        if (dependent == nullptr) continue;
        output.PushBack(dependent->uri);
    }
    return {};
}

Base::Result<void> DependencyGraph::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (state_ == nullptr || changed.Empty()) return {};

    Base::Vector<Base::String> queue(allocator_);
    Base::HashSet<Base::String> visited(allocator_);
    Base::Result<Base::String> changedKey =
        MakeKey(changed, *allocator_);
    if (!changedKey) return changedKey.GetStatus();
    queue.PushBack(changedKey.Value());

    std::uint32_t cursor = 0U;
    while (cursor < queue.Size()) {
        Base::String key = queue[cursor++];
        typename Base::HashSet<Base::String>::InsertResult
            inserted = visited.Insert(key);
        if (!inserted.inserted) continue;

        const DependencyGraph::State::Node* node = state_->nodes.Find(key);
        Base::ResourceUri uri = node != nullptr
            ? node->uri
            : changed;
        output.PushBack(uri);
        if (node == nullptr) continue;
        for (const Base::String& dependent : node->dependents) {
            if (visited.Contains(dependent)) continue;
            queue.PushBack(dependent);
        }
    }
    return {};
}

std::uint32_t DependencyGraph::NodeCount() const noexcept {
    return state_ != nullptr ? state_->nodes.Size() : 0U;
}

std::uint64_t DependencyGraph::Generation() const noexcept {
    return state_ != nullptr ? state_->generation : 0U;
}

struct DocumentCache::State {
    struct Entry {
        explicit Entry(Base::IAllocator&) noexcept {}

        Base::ResourceUri uri;
        std::uint64_t entrySize = 0U;
        CompiledDocument document;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    State(
        Base::IAllocator& allocator,
        const DocumentCacheLimits& valueLimits) noexcept
        : allocator(&allocator),
          entries(&allocator),
          graph(&allocator),
          limits(valueLimits) {}

    bool EraseEntry(
        const Base::ResourceUri& uri,
        bool eviction) noexcept {
        Base::Result<Base::String> key = MakeKey(uri, *allocator);
        if (!key) return false;
        Entry* entry = entries.Find(key.Value());
        if (entry == nullptr) return false;
        compiledBytes -= entry->entrySize;
        entries.Erase(key.Value());
        graph.Remove(uri);
        if (eviction) ++evictions;
        else ++invalidations;
        ++generation;
        return true;
    }

    void EvictToLimits() noexcept {
        while ((limits.maxEntries != 0U &&
                entries.Size() > limits.maxEntries) ||
               (limits.maxCompiledBytes != 0U &&
                compiledBytes > limits.maxCompiledBytes)) {
            const typename Base::HashMap<Base::String, Entry>::Entry*
                oldest = nullptr;
            for (const auto& current : entries) {
                if (oldest == nullptr ||
                    current.Value().lastAccess <
                        oldest->Value().lastAccess) {
                    oldest = &current;
                }
            }
            if (oldest == nullptr) break;
            Base::ResourceUri uri = oldest->Value().uri;
            if (!EraseEntry(uri, true)) break;
        }
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Entry> entries;
    DependencyGraph graph;
    DocumentCacheLimits limits;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t accessSequence = 0U;
    std::uint64_t hits = 0U;
    std::uint64_t misses = 0U;
    std::uint64_t stores = 0U;
    std::uint64_t invalidations = 0U;
    std::uint64_t evictions = 0U;
    std::uint64_t generation = 0U;
};

DocumentCache::DocumentCache(
    Base::IAllocator* allocator,
    const DocumentCacheLimits& limits) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state_, *allocator_, limits));
}

DocumentCache::~DocumentCache() noexcept {
    if (state_ == nullptr) return;
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    state_ = nullptr;
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_),
      state_(other.state_) {
    other.state_ = nullptr;
    other.allocator_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state_);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.state_ = nullptr;
    other.allocator_ = nullptr;
    return *this;
}

Base::Result<DocumentCacheLookup> DocumentCache::Lookup(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    DocumentCacheLookup result;
    (void)domain;
    (void)limits;
    if (state_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    DocumentCache::State::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr) {
        ++state_->misses;
        return result;
    }
    if (entry->sourceRevision != sourceRevision ||
        entry->sourceIdentity != sourceIdentity) {
        ++state_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }

    if (!entry->document.IsValid()) {
        ++state_->misses;
        return result;
    }

    entry->lastAccess = ++state_->accessSequence;
    ++state_->hits;
    result.hit = true;
    result.sourceRevision = entry->sourceRevision;
    result.document = &entry->document;
    return result;
}

namespace {
template<class TDoc>
Base::Result<void> StoreInternal(
    DocumentCache::State* state,
    Base::IAllocator* allocator,
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    TDoc&& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (state == nullptr || uri.Empty() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML document cache entry is invalid");
    }
    CompiledDocument doc;
    if constexpr (std::is_rvalue_reference_v<TDoc&&>) {
        doc = std::move(document);
    } else {
        Base::Result<CompiledDocument> cloned = document.Clone();
        if (!cloned) return cloned.GetStatus();
        doc = std::move(cloned).Value();
    }

    const std::uint64_t serializedSize =
        static_cast<std::uint64_t>(doc.Nodes().Size()) * sizeof(Node) + sizeof(CompiledDocument);

    Base::Result<Base::String> key = MakeKey(uri, *allocator);
    if (!key) return key.GetStatus();
    DocumentCache::State::Entry* existing = state->entries.Find(key.Value());
    if (existing != nullptr) {
        state->compiledBytes -= existing->entrySize;
        existing->uri = uri;
        existing->entrySize = serializedSize;
        existing->document = std::move(doc);
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++state->accessSequence;
        state->compiledBytes += existing->entrySize;
    } else {
        DocumentCache::State::Entry entry(*allocator);
        entry.uri = uri;
        entry.entrySize = serializedSize;
        entry.document = std::move(doc);
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++state->accessSequence;
        state->compiledBytes += entry.entrySize;
        typename Base::HashMap<Base::String, DocumentCache::State::Entry>::InsertResult
            inserted = state->entries.Insert(
                std::move(key).Value(), std::move(entry));
        if (!inserted.inserted) {
            state->compiledBytes -= serializedSize;
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Document cache entry already exists");
        }
    }
    Base::Result<void> graph =
        state->graph.Update(uri, dependencies);
    if (!graph) {
        state->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++state->stores;
    ++state->generation;
    state->EvictToLimits();
    return {};
}
} // namespace

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const CompiledDocument& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    return StoreInternal(state_, allocator_, uri, sourceRevision, sourceIdentity, document, dependencies);
}

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    CompiledDocument&& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    return StoreInternal(state_, allocator_, uri, sourceRevision, sourceIdentity, std::move(document), dependencies);
}

Base::Result<std::uint32_t> DocumentCache::Invalidate(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (state_ == nullptr || uri.Empty()) return 0U;
    Base::Vector<Base::ResourceUri> affected(allocator_);
    if (includeDependents) {
        Base::Result<void> collected =
            state_->graph.CollectAffected(uri, affected);
        if (!collected) return collected.GetStatus();
    } else {
        affected.PushBack(uri);
    }
    std::uint32_t count = 0U;
    for (std::uint32_t index = affected.Size();
         index > 0U;
         --index) {
        const Base::ResourceUri& affectedUri = affected[index - 1U];
        if (state_->EraseEntry(affectedUri, false)) {
            ++count;
        } else {
            static_cast<void>(state_->graph.Remove(affectedUri));
        }
    }
    return count;
}

void DocumentCache::Clear() noexcept {
    if (state_ == nullptr) return;
    state_->entries.Clear();
    state_->graph.Clear();
    state_->compiledBytes = 0U;
    ++state_->generation;
}

bool DocumentCache::Contains(
    const Base::ResourceUri& uri) const noexcept {
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    return key && state_->entries.Contains(key.Value());
}

bool DocumentCache::GetSourceRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) const noexcept {
    revision = 0U;
    if (state_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return false;
    const DocumentCache::State::Entry* entry = state_->entries.Find(key.Value());
    if (entry == nullptr || entry->sourceIdentity != sourceIdentity) {
        return false;
    }
    revision = entry->sourceRevision;
    return true;
}

Base::Result<void> DocumentCache::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    return state_ != nullptr
        ? state_->graph.CollectAffected(changed, output)
        : Base::Result<void>{};
}

const DependencyGraph& DocumentCache::Dependencies() const noexcept {
    static const DependencyGraph empty;
    return state_ != nullptr ? state_->graph : empty;
}

DocumentCacheStatistics DocumentCache::Statistics() const noexcept {
    DocumentCacheStatistics result;
    if (state_ == nullptr) return result;
    result.entryCount = state_->entries.Size();
    result.compiledBytes = state_->compiledBytes;
    result.hitCount = state_->hits;
    result.missCount = state_->misses;
    result.storeCount = state_->stores;
    result.invalidationCount = state_->invalidations;
    result.evictionCount = state_->evictions;
    result.generation = state_->generation;
    return result;
}

const DocumentCacheLimits& DocumentCache::Limits() const noexcept {
    static const DocumentCacheLimits empty{};
    return state_ != nullptr ? state_->limits : empty;
}
} // namespace Aero::Markup

// ============================================================================
// SECTION: ReloadCoordinator
// ============================================================================

namespace Aero::Markup {
struct ReloadCoordinatorState final {
    struct RevisionRecord  {
        Base::ResourceUri uri;
        std::uint64_t revision = 0U;
    };

    ReloadCoordinatorState(
        View& valueView,
        Base::IAllocator& valueAllocator,
        GuiRuntime* valueGui) noexcept
        : view(&valueView),
          allocator(&valueAllocator),
          gui(valueGui),
          revisions(&valueAllocator) {}

    Base::Result<std::uint64_t> ReadRevision(
        const Base::ResourceUri& uri) noexcept {
        if (view == nullptr || gui == nullptr || uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload source is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t revision = 0U;
        Base::Result<void> queried = gui->QuerySource(
            uri, sourceIdentity, revision);
        return queried
            ? Base::Result<std::uint64_t>(revision)
            : Base::Result<std::uint64_t>(queried.GetStatus());
    }

    static bool HasTracked(
        const Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        for (const RevisionRecord& record : records) {
            if (record.uri == uri) return true;
        }
        return false;
    }

    Base::Result<void> AddTracked(
        Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        if (uri.Empty() || HasTracked(records, uri)) return {};
        if (view == nullptr || gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload View is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t currentRevision = 0U;
        Base::Result<void> queried = gui->QuerySource(
            uri, sourceIdentity, currentRevision);
        if (!queried) return queried.GetStatus();
        std::uint64_t revision = currentRevision;
        static_cast<void>(gui->TryGetCachedRevision(
            uri, sourceIdentity, revision));
        RevisionRecord record;
        record.uri = uri;
        record.revision = revision;
        records.PushBack(std::move(record));
        return {};
    }

    Base::Result<void> BuildTrackedSources(
        const XamlDocument& document,
        Base::ResourceUri& resolvedRoot,
        Base::Vector<RevisionRecord>& output) noexcept {
        output.Clear();
        if (view == nullptr || !document.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload document is unavailable");
        }
        if (!document.CanonicalUri().Empty()) {
            resolvedRoot = document.CanonicalUri();
        }
        if (resolvedRoot.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML reload root URI is empty");
        }
        output.Reserve(1U + document.Dependencies().Size());
        Base::Result<void> root = AddTracked(output, resolvedRoot);
        if (!root) return root.GetStatus();
        for (const Base::ResourceUri& dependency : document.Dependencies()) {
            Base::Result<void> tracked = AddTracked(output, dependency);
            if (!tracked) return tracked.GetStatus();
        }
        return {};
    }

    Base::Result<ReloadResult> ReloadFor(
        const Base::ResourceUri& changed,
        Diagnostics::IDiagnosticSink* diagnostics) noexcept {
        if (!active || view == nullptr || view->GetContent() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload coordinator is not active");
        }
        if (gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload Gui state is unavailable");
        }
        Base::Result<std::uint32_t> invalidated =
            gui->Invalidate(changed, true);
        if (!invalidated) return invalidated.GetStatus();
        const std::uint32_t invalidatedCount = invalidated.Value();

        XamlReader reader(view->GetGui());
        Base::Result<XamlDocument> replacement =
            reader.Load(rootUri.Canonical(), {}, diagnostics);
        if (!replacement) return replacement.GetStatus();
        Base::ResourceUri replacementRoot = rootUri;
        Base::Vector<RevisionRecord> replacementRevisions(allocator);
        Base::Result<void> tracked = BuildTrackedSources(
            replacement.Value(), replacementRoot, replacementRevisions);
        if (!tracked) return tracked.GetStatus();
        Aero::Size targetSize = availableSize;
        if ((targetSize.width <= 0.0 || targetSize.height <= 0.0) &&
            view != nullptr) {
            targetSize = view->GetSize();
        }
        if (!view->SetContent(
            std::move(replacement).Value(), targetSize)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState, "SetContent failed");
        }
        rootUri = replacementRoot;
        revisions = std::move(replacementRevisions);

        ReloadResult result;
        result.changed = true;
        result.reloaded = true;
        result.invalidatedDocuments = invalidatedCount;
        result.generation = ++generation;
        result.changedUri = changed;
        return result;
    }

    View* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    GuiRuntime* gui = nullptr;
    Base::ResourceUri rootUri;
    Aero::Size availableSize;
    Base::Vector<RevisionRecord> revisions;
    std::uint64_t consumedXamlChangeGeneration = 0U;
    std::uint64_t generation = 0U;
    bool active = false;
};

ReloadCoordinator::ReloadCoordinator(
    View& view,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Gui& gui = view.GetGui();
    GuiRuntime* guiState = gui.state_.Get();
    ReloadCoordinatorState* state = nullptr;
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::Markup, state, view, *allocator_, guiState));
    state_ = state;
}

ReloadCoordinator::~ReloadCoordinator() noexcept {
    if (state_ == nullptr) return;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state);
    state_ = nullptr;
}

ReloadCoordinator::ReloadCoordinator(
    ReloadCoordinator&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

ReloadCoordinator& ReloadCoordinator::operator=(
    ReloadCoordinator&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        auto* state = static_cast<ReloadCoordinatorState*>(state_);
        Base::FreeObject(*allocator_, Base::MemoryTag::Markup, state);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}

Base::Result<void> ReloadCoordinator::Start(
    Base::StringView rootUri,
    Aero::Size availableSize,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || state->view == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "XAML reload requires a View");
    }
    if (state->active || state->view->GetContent() != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML reload requires an unmounted View");
    }
    Base::Result<Base::ResourceUri> parsed =
        Base::ResourceUri::Parse(rootUri);
    if (!parsed) return parsed.GetStatus();
    XamlReader reader(state->view->GetGui());
    Base::Result<XamlDocument> document =
        reader.Load(rootUri, {}, diagnostics);
    if (!document) return document.GetStatus();
    Base::ResourceUri resolvedRoot = parsed.Value();
    Base::Vector<ReloadCoordinatorState::RevisionRecord> revisions(
        state->allocator);
    Base::Result<void> tracked = state->BuildTrackedSources(
        document.Value(), resolvedRoot, revisions);
    if (!tracked) return tracked.GetStatus();
    if (!state->view->SetContent(
        std::move(document).Value(), availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, "SetContent failed");
    }

    state->rootUri = resolvedRoot;
    state->revisions = std::move(revisions);
    state->availableSize = availableSize;
    state->consumedXamlChangeGeneration = state->gui != nullptr
        ? state->gui->xamlChangeGeneration : 0U;
    state->active = true;
    return {};
}

void ReloadCoordinator::Stop() noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) return;
    state->active = false;
    state->revisions.Clear();
    state->rootUri = {};
}

Base::Result<ReloadResult> ReloadCoordinator::Poll(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    ReloadResult noChange;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || !state->active) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is not active");
    }
    noChange.generation = state->generation;
    if (state->gui != nullptr &&
        state->gui->xamlChangeGeneration >
            state->consumedXamlChangeGeneration) {
        const std::uint64_t latest = state->gui->xamlChangeGeneration;
        if (state->gui->xamlChangesLost) {
            state->consumedXamlChangeGeneration = latest;
            return state->ReloadFor(state->rootUri, diagnostics);
        }
        for (const XamlProviderChangeRecord& change :
             state->gui->xamlChanges) {
            if (change.generation <=
                    state->consumedXamlChangeGeneration) {
                continue;
            }
            state->consumedXamlChangeGeneration = change.generation;
            if (change.uri.Empty()) {
                return state->ReloadFor(state->rootUri, diagnostics);
            }
            for (const ReloadCoordinatorState::RevisionRecord& record :
                 state->revisions) {
                if (record.uri == change.uri) {
                    return state->ReloadFor(change.uri, diagnostics);
                }
            }
        }
        state->consumedXamlChangeGeneration = latest;
    }
    for (const ReloadCoordinatorState::RevisionRecord& record : state->revisions) {
        Base::Result<std::uint64_t> current =
            state->ReadRevision(record.uri);
        if (!current) return current.GetStatus();
        if (current.Value() == record.revision) continue;
        return state->ReloadFor(record.uri, diagnostics);
    }
    return noChange;
}

Base::Result<ReloadResult> ReloadCoordinator::Reload(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is unavailable");
    }
    return state->ReloadFor(state->rootUri, diagnostics);
}

Base::Result<ReloadResult>
ReloadCoordinator::NotifySourceChanged(
    const Base::ResourceUri& changedUri,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || changedUri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML reload changed URI is invalid");
    }
    return state->ReloadFor(changedUri, diagnostics);
}

bool ReloadCoordinator::IsActive() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr && state->active;
}

const Base::ResourceUri& ReloadCoordinator::RootUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->rootUri : empty;
}

std::uint64_t ReloadCoordinator::Generation() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->generation : 0U;
}

std::uint32_t ReloadCoordinator::TrackedSourceCount() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->revisions.Size() : 0U;
}
} // namespace Aero::Markup

// ============================================================================
// SECTION: XamlReader entry points
// ============================================================================

namespace Aero::Markup {
namespace {
struct XamlLoadScope {
    XamlLoadScope(
        Threading::Dispatcher& dispatcher,
        GuiSchema& schema,
        DocumentCache& documents) noexcept
        : factory(
              dispatcher,
              (
                  schema.Metadata()).DependencyProperties(),
              schema.Metadata()) {
        load.documentCache = &documents;
        load.dispatcher = &dispatcher;
        load.dependencyProperties =
            &(
                schema.Metadata()).DependencyProperties();
        load.effectCommitMode = EffectCommitMode::Deferred;
    }

    Meta::ObjectFactoryScope factory;
    LoadState load;
};
} // namespace

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Load(
        &scope.load, uri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    Aero::ResourceDictionary& resources,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    scope.load.resources = &resources;
    scope.load.fallbackResources = &resources;
    return state.Load(
        &scope.load, uri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Parse(
        &scope.load, source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadComponentCore(
    Base::StringView uri,
    Meta::TypeId expectedRoot,
    ResourceDictionary* resources,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<XamlDocument> loaded = resources != nullptr
        ? Load(uri, *resources, settings, diagnostics)
        : Load(uri, settings, diagnostics);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || gui_ == nullptr || expectedRoot == Meta::InvalidTypeId) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    const GuiRuntime& state = *gui_->state_;
    if (root->RuntimeType() != expectedRoot &&
        !state.schema.Metadata().Types().IsDerivedFrom(
            root->RuntimeType(), expectedRoot)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::LoadComponentInto(
    Base::Ref<Base::Object> existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics,
    ResourceDictionary* resources) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML component loading");
    }
    if (!existingRoot) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component requires an existing root object");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    if (resources != nullptr) {
        // Ambient lookup only. `resources` is the document dictionary; wiring
        // Application.Resources there lets LoadComponent mutate/seal app-wide
        // styles and drops Button/Thumb hover storyboards.
        scope.load.fallbackResources = resources;
    }
    Base::Result<XamlDocument> loaded = state.LoadComponentInto(
        &scope.load,
        *existingRoot,
        uri,
        settings,
        diagnostics);
    if (!loaded) return loaded.GetStatus();
    if (loaded.Value().Root().Get() != existingRoot.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML component replaced its existing root object");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML parsing");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.Parse(
        &scope.load, source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before compiled XAML loading");
    }
    GuiRuntime& state = *gui_->state_;
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.LoadCompiled(
        &scope.load, bytes, originUri, XamlReaderSettings{});
}
} // namespace Aero::Markup

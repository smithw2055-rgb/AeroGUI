#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Gui.hpp>
#include "gui/GuiRuntime.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/markup/XamlObjectWriterState.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/View.hpp>

#include <atomic>
#include <new>
#include <utility>

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


#include <Aero/Base/Result.hpp>




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
#include "gui/markup/XamlSchema.hpp"


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
#include <Aero/Gui.hpp>


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

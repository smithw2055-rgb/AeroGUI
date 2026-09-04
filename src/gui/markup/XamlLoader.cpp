#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include <Aero/VisualStateManager.hpp>
// Consolidated implementation. Keep sections ordered by dependency.

#include <atomic>

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



#include "gui/markup/XamlCompiledDocument.inl"
#include "gui/markup/XamlDocumentCache.inl"
#include "gui/markup/XamlObjectLoader.inl"

// ===== Resources =====




#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Controls/UserControl.hpp>

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
    return static_cast<ResourceDictionary&>(scopeOwner)
        .Add(key, value);
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
    return element->GetResources().Add(key, value);
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
    return element->RegisterName(name, object);
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
        SchemaPrivate::AddResourceScope(schema, {
            ResourceDictionary::StaticTypeId(),
            true,
            &AddResource,
            &ResolveDictionaryScope,
            this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = SchemaPrivate::AddResourceScope(schema, {
        FrameworkElement::StaticTypeId(),
        true,
        &AddFrameworkResource,
        &ResolveFrameworkScope,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = SchemaPrivate::AddNameScope(schema, {
        FrameworkElement::StaticTypeId(),
        false,
        &RegisterFrameworkName,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    status = SchemaPrivate::AddNameScope(schema, {
        ::Aero::Controls::UserControl::StaticTypeId(),
        true,
        &RegisterFrameworkName,
        this});
    if (!status) {
        schema_ = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


// ===== XamlDocument =====

#include <Aero/Markup/XamlReader.hpp>

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
XamlDocumentRuntimeLifetime(
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

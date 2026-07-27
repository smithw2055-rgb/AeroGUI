#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Markup/CompiledDocument.hpp>
#include <Aero/UiDocument.hpp>

#include <atomic>
#include <cstdint>

namespace Aero::Presentation {
class BindingManager;
class ResourceDictionary;
}

namespace Aero::Core {
class DependencyPropertyRegistry;
class Dispatcher;
class EffectiveValueEngine;
}

namespace Aero::Markup {

class Schema;

class AERO_API EffectLifetime final : public Base::Object {
public:
    EffectLifetime() noexcept = default;
    ~EffectLifetime() noexcept override = default;

    bool IsActive() const noexcept {
        return active_.load(std::memory_order_acquire);
    }
    void Invalidate() noexcept {
        active_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> active_{true};
};

enum class EffectCommitMode : std::uint8_t {
    Immediate = 0U,
    Deferred
};

struct Source final {
    Base::ResourceUri uri;
    Base::Vector<std::uint8_t> bytes;
    std::uint64_t revision = 0U;

    Base::StringView Text() const noexcept {
        return Base::StringView(
            reinterpret_cast<const char*>(bytes.Data()),
            bytes.Size());
    }
};

class AERO_API ISourceProvider {
public:
    virtual ~ISourceProvider() = default;

    virtual Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Markup source provider does not expose revision probes");
    }
    virtual std::uint64_t CacheIdentity() const noexcept {
        return Base::DefaultHash<const ISourceProvider*>{}(this);
    }
};

using SourceLoadCallback = Base::Result<Source> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;
using SourceRevisionCallback = Base::Result<std::uint64_t> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;

class AERO_API SourceProviderAdapter final
    : public ISourceProvider {
public:
    SourceProviderAdapter() noexcept = default;
    SourceProviderAdapter(
        SourceLoadCallback load,
        void* context = nullptr,
        SourceRevisionCallback revision = nullptr,
        std::uint64_t cacheIdentity = 0U) noexcept
        : load_(load), revision_(revision), context_(context),
          cacheIdentity_(cacheIdentity) {}

    bool IsValid() const noexcept { return load_ != nullptr; }

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override {
        if (load_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup source provider has no load callback");
        }
        return load_(uri, context_);
    }
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override {
        return revision_ != nullptr
            ? revision_(uri, context_)
            : ISourceProvider::Revision(uri);
    }
    std::uint64_t CacheIdentity() const noexcept override {
        return cacheIdentity_ != 0U
            ? cacheIdentity_
            : ISourceProvider::CacheIdentity();
    }

private:
    SourceLoadCallback load_ = nullptr;
    SourceRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
    std::uint64_t cacheIdentity_ = 0U;
};

struct SourceProviderResolution final {
    ISourceProvider* provider = nullptr;
    std::uint64_t cacheIdentity = 0U;
};

struct SourceProviderRegistration final {
    Base::String scheme;
    Base::String assembly;
    ISourceProvider* provider = nullptr;
};

class AERO_API SourceProviderRegistry final {
public:
    Base::Result<void> TryRegister(
        ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;
    Base::Result<void> TryRegister(
        SourceProviderAdapter& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept {
        if (!provider.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Markup source provider is invalid");
        }
        return TryRegister(
            static_cast<ISourceProvider&>(provider),
            scheme,
            assembly);
    }
    Base::Result<SourceProviderResolution> ResolveDetailed(
        const Base::ResourceUri& uri) const noexcept;
    Base::Result<ISourceProvider*> Resolve(
        const Base::ResourceUri& uri) const noexcept;

    std::uint32_t ProviderCount() const noexcept {
        return registrations_.Size();
    }

private:
    Base::Vector<SourceProviderRegistration> registrations_;
};

class AERO_API EmbeddedSourceProvider final
    : public ISourceProvider {
public:
    Base::Result<void> TryAdd(
        const Base::ResourceUri& uri,
        Base::Span<const std::uint8_t> bytes,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> TryAddText(
        const Base::ResourceUri& uri,
        Base::StringView text,
        std::uint64_t revision = 1U) noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override {
        return cacheIdentity_;
    }

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t SourceCount() const noexcept {
        return entries_.Size();
    }

private:
    struct Entry final {
        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> bytes;
        std::uint64_t revision = 0U;
    };

    Base::Vector<Entry> entries_;
    std::uint64_t cacheIdentity_ = UINT64_C(0xA3E0E4BEDDED0001);
    bool frozen_ = false;
};

class AERO_API FileSourceProvider final
    : public ISourceProvider {
public:
    explicit FileSourceProvider(
        std::uint64_t maxFileBytes =
            64ULL * 1024ULL * 1024ULL) noexcept
        : maxFileBytes_(maxFileBytes) {}

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override {
        return Base::MixHash64(
            UINT64_C(0xA3E0F11E00000001) ^ maxFileBytes_);
    }

private:
    std::uint64_t maxFileBytes_ = 0U;
};

struct DocumentCacheLimits final {
    std::uint32_t maxEntries = 256U;
    std::uint64_t maxCompiledBytes =
        64ULL * 1024ULL * 1024ULL;
};

struct DocumentCacheStatistics final {
    std::uint32_t entryCount = 0U;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t hitCount = 0U;
    std::uint64_t missCount = 0U;
    std::uint64_t storeCount = 0U;
    std::uint64_t invalidationCount = 0U;
    std::uint64_t evictionCount = 0U;
    std::uint64_t generation = 0U;
};

struct DocumentCacheLookup final {
    bool hit = false;
    std::uint64_t sourceRevision = 0U;
    CompiledDocument document;
};

class AERO_API DependencyGraph final {
public:
    explicit DependencyGraph(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~DependencyGraph() noexcept;

    DependencyGraph(DependencyGraph&& other) noexcept;
    DependencyGraph& operator=(
        DependencyGraph&& other) noexcept;

    DependencyGraph(const DependencyGraph&) = delete;
    DependencyGraph& operator=(const DependencyGraph&) = delete;

    Base::Result<void> Update(
        const Base::ResourceUri& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;
    bool Remove(
        const Base::ResourceUri& document) noexcept;
    void Clear() noexcept;

    Base::Result<void> CopyDependencies(
        const Base::ResourceUri& document,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CopyDependents(
        const Base::ResourceUri& dependency,
        Base::Vector<Base::ResourceUri>& output) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    std::uint32_t NodeCount() const noexcept;
    std::uint64_t Generation() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

class AERO_API DocumentCache final {
public:
    explicit DocumentCache(
        Base::IAllocator* allocator = nullptr,
        const DocumentCacheLimits& limits = {}) noexcept;
    ~DocumentCache() noexcept;

    DocumentCache(DocumentCache&& other) noexcept;
    DocumentCache& operator=(
        DocumentCache&& other) noexcept;

    DocumentCache(const DocumentCache&) = delete;
    DocumentCache& operator=(const DocumentCache&) = delete;

    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const Core::MetadataDomain& domain,
        const CompiledDocumentLimits& limits = {}) noexcept {
        return Lookup(
            uri, sourceRevision, 0U, domain, limits);
    }
    Base::Result<DocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const Core::MetadataDomain& domain,
        const CompiledDocumentLimits& limits = {}) noexcept;
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept {
        return Store(
            uri, sourceRevision, 0U,
            document, dependencies);
    }
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const CompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents = true) noexcept;
    void Clear() noexcept;

    bool Contains(
        const Base::ResourceUri& uri) const noexcept;
    bool TryGetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t& revision) const noexcept {
        return TryGetSourceRevision(uri, 0U, revision);
    }
    bool TryGetSourceRevision(
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) const noexcept;
    Base::Result<void> CollectAffected(
        const Base::ResourceUri& changed,
        Base::Vector<Base::ResourceUri>& output) const noexcept;

    const DependencyGraph& Dependencies() const noexcept;
    DocumentCacheStatistics Statistics() const noexcept;
    const DocumentCacheLimits& Limits() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

struct LoadPolicy final {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct LoadLimits final {
    XmlTokenizerLimits xml;
    CompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes = 16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct LoadOptions final {
    LoadPolicy policy;
    LoadLimits limits;
    Base::ResourceUri baseUri;
    const Presentation::ResourceDictionary* resources = nullptr;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::ResourceDictionary* fallbackResources = nullptr;
    DocumentCache* documentCache = nullptr;
    Core::Dispatcher* dispatcher = nullptr;
    Core::DependencyPropertyRegistry* dependencyProperties = nullptr;
    Base::Object* templatedParent = nullptr;
    Base::Ref<EffectLifetime> effectLifetime;
    EffectCommitMode effectCommitMode =
        EffectCommitMode::Immediate;
};

class AERO_API Loader final {
public:
    Loader(
        Schema& schema,
        SourceProviderRegistry& providers,
        Core::IDiagnosticSink* diagnostics = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~Loader() noexcept;

    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;
    Loader(Loader&&) = delete;
    Loader& operator=(Loader&&) = delete;

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> Load(
        const Base::ResourceUri& uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const LoadOptions& options = {}) noexcept;
    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const LoadOptions& options = {}) noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup

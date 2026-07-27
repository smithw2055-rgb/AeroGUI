#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>

#include <cstdint>

namespace Aero::Core {
class MetadataDomain;
}

namespace Aero::Markup {

struct XamlDocumentCacheLimits final {
    std::uint32_t maxEntries = 256U;
    std::uint64_t maxCompiledBytes = 64ULL * 1024ULL * 1024ULL;
};

struct XamlDocumentCacheStatistics final {
    std::uint32_t entryCount = 0U;
    std::uint64_t compiledBytes = 0U;
    std::uint64_t hitCount = 0U;
    std::uint64_t missCount = 0U;
    std::uint64_t storeCount = 0U;
    std::uint64_t invalidationCount = 0U;
    std::uint64_t evictionCount = 0U;
    std::uint64_t generation = 0U;
};

struct XamlDocumentCacheLookup final {
    bool hit = false;
    std::uint64_t sourceRevision = 0U;
    XamlCompiledDocument document;
};

// Canonical URI dependency graph used by the document cache and hot-reload
// coordinator. Edges point from a document to the resources it consumed; the
// reverse edges allow a changed resource to invalidate every dependent root.
// The graph is not internally synchronized; callers use one owner thread or
// provide external synchronization when sharing it across views.
class AERO_API XamlDependencyGraph final {
public:
    explicit XamlDependencyGraph(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlDependencyGraph() noexcept;

    XamlDependencyGraph(XamlDependencyGraph&& other) noexcept;
    XamlDependencyGraph& operator=(XamlDependencyGraph&& other) noexcept;

    XamlDependencyGraph(const XamlDependencyGraph&) = delete;
    XamlDependencyGraph& operator=(const XamlDependencyGraph&) = delete;

    Base::Result<void> Update(
        const Base::ResourceUri& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;
    bool Remove(const Base::ResourceUri& document) noexcept;
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

// Shared, schema-aware cache of immutable replay documents. Cache entries are
// validated against the current metadata domain on every lookup. The cache owns
// serialized AXIR rather than instantiated objects, so one entry can safely be
// replayed by multiple RuntimeView instances. Cache access is not internally
// synchronized; multi-threaded hosts must serialize access explicitly.
class AERO_API XamlDocumentCache final {
public:
    explicit XamlDocumentCache(
        Base::IAllocator* allocator = nullptr,
        const XamlDocumentCacheLimits& limits = {}) noexcept;
    ~XamlDocumentCache() noexcept;

    XamlDocumentCache(XamlDocumentCache&& other) noexcept;
    XamlDocumentCache& operator=(XamlDocumentCache&& other) noexcept;

    XamlDocumentCache(const XamlDocumentCache&) = delete;
    XamlDocumentCache& operator=(const XamlDocumentCache&) = delete;

    Base::Result<XamlDocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const Core::MetadataDomain& domain,
        const XamlCompiledDocumentLimits& limits = {}) noexcept {
        return Lookup(uri, sourceRevision, 0U, domain, limits);
    }
    Base::Result<XamlDocumentCacheLookup> Lookup(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const Core::MetadataDomain& domain,
        const XamlCompiledDocumentLimits& limits = {}) noexcept;
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        const XamlCompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept {
        return Store(uri, sourceRevision, 0U, document, dependencies);
    }
    Base::Result<void> Store(
        const Base::ResourceUri& uri,
        std::uint64_t sourceRevision,
        std::uint64_t sourceIdentity,
        const XamlCompiledDocument& document,
        Base::Span<const Base::ResourceUri> dependencies) noexcept;

    Base::Result<std::uint32_t> Invalidate(
        const Base::ResourceUri& uri,
        bool includeDependents = true) noexcept;
    void Clear() noexcept;

    bool Contains(const Base::ResourceUri& uri) const noexcept;
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

    const XamlDependencyGraph& Dependencies() const noexcept;
    XamlDocumentCacheStatistics Statistics() const noexcept;
    const XamlDocumentCacheLimits& Limits() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup

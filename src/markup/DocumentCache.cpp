#include "Loader.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Meta/MetadataDomain.hpp>

#include <new>
#include <utility>
#include "../ui/RuntimeManagers.hpp"

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
    Base::Result<void> assigned = key.TryAssign(uri.Canonical());
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

struct DependencyGraph::Impl final {
    struct Node final {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit Impl(Base::IAllocator& allocator) noexcept
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
        Base::Result<typename Base::HashMap<Base::String, Node>::InsertResult>
            inserted = nodes.TryInsert(
                std::move(key).Value(), std::move(node));
        if (!inserted) return inserted.GetStatus();
        return &inserted.Value().entry->Value();
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Node> nodes;
    std::uint64_t generation = 0U;
};

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_);
}

DependencyGraph::~DependencyGraph() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<void> DependencyGraph::Update(
    const Base::ResourceUri& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph is unavailable");
    }
    Base::Result<Base::String> documentKey =
        MakeKey(document, *allocator_);
    if (!documentKey) return documentKey.GetStatus();
    Base::Result<Impl::Node*> documentNode =
        impl_->EnsureNode(document);
    if (!documentNode) return documentNode.GetStatus();

    Base::Vector<Base::String> newDependencies(allocator_);
    Base::Result<void> reserved =
        newDependencies.TryReserve(dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::ResourceUri& dependencyUri : dependencies) {
        if (dependencyUri.Empty() || dependencyUri == document) continue;
        Base::Result<Base::String> dependencyKey =
            MakeKey(dependencyUri, *allocator_);
        if (!dependencyKey) return dependencyKey.GetStatus();
        if (ContainsKey(newDependencies, dependencyKey.Value().View())) {
            continue;
        }
        Base::Result<Impl::Node*> dependencyNode =
            impl_->EnsureNode(dependencyUri);
        if (!dependencyNode) return dependencyNode.GetStatus();
        Base::Result<void> appended = newDependencies.TryPushBack(
            std::move(dependencyKey).Value());
        if (!appended) return appended.GetStatus();
    }

    // Prepare reverse-edge key ownership and vector capacity before mutating
    // any edge. No hash-map insertions occur after this point, so node
    // references remain stable even when EnsureNode() previously rehashed.
    Base::Vector<Base::String> reverseKeys(allocator_);
    Base::Result<void> reverseReserved =
        reverseKeys.TryReserve(newDependencies.Size());
    if (!reverseReserved) return reverseReserved.GetStatus();
    for (const Base::String& dependencyKey : newDependencies) {
        Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML dependency graph lost a prepared node");
        }
        Base::Result<void> reverseCapacity =
            dependency->dependents.TryReserve(
                dependency->dependents.Size() + 1U);
        if (!reverseCapacity) return reverseCapacity.GetStatus();
        Base::String reverseKey(allocator_);
        Base::Result<void> copied = reverseKey.TryAssign(
            documentKey.Value().View());
        if (!copied) return copied.GetStatus();
        Base::Result<void> stored = reverseKeys.TryPushBack(
            std::move(reverseKey));
        if (!stored) return stored.GetStatus();
    }

    Impl::Node* node = impl_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        Impl::Node* dependency = impl_->nodes.Find(oldDependency);
        if (dependency == nullptr) continue;
        RemoveKey(
            dependency->dependents,
            documentKey.Value().View());
        if (!ContainsKey(newDependencies, oldDependency.View()) &&
            dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            impl_->nodes.Erase(oldDependency);
        }
    }
    node->dependencies = std::move(newDependencies);
    for (std::uint32_t index = 0U;
         index < node->dependencies.Size();
         ++index) {
        Impl::Node* dependency =
            impl_->nodes.Find(node->dependencies[index]);
        if (dependency == nullptr) continue;
        if (ContainsKey(
                dependency->dependents,
                documentKey.Value().View())) {
            continue;
        }
        Base::Result<void> reverse =
            dependency->dependents.TryPushBack(
                std::move(reverseKeys[index]));
        if (!reverse) return reverse.GetStatus();
    }
    ++impl_->generation;
    return {};
}

bool DependencyGraph::Remove(
    const Base::ResourceUri& document) noexcept {
    if (impl_ == nullptr || document.Empty()) return false;
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return false;
    Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    if (!previousDependencies.TryAppend(
            node->dependencies.AsSpan())) {
        return false;
    }
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        RemoveKey(dependency->dependents, key.Value().View());
        if (dependency->dependencies.Empty() &&
            dependency->dependents.Empty()) {
            impl_->nodes.Erase(dependencyKey);
        }
    }
    if (node->dependents.Empty()) {
        impl_->nodes.Erase(key.Value());
    }
    ++impl_->generation;
    return true;
}

void DependencyGraph::Clear() noexcept {
    if (impl_ == nullptr) return;
    impl_->nodes.Clear();
    ++impl_->generation;
}

Base::Result<void> DependencyGraph::CopyDependencies(
    const Base::ResourceUri& document,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || document.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(document, *allocator_);
    if (!key) return key.GetStatus();
    const Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.TryReserve(node->dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependencyKey : node->dependencies) {
        const Impl::Node* dependency = impl_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        Base::Result<void> pushed = output.TryPushBack(dependency->uri);
        if (!pushed) return pushed.GetStatus();
    }
    return {};
}

Base::Result<void> DependencyGraph::CopyDependents(
    const Base::ResourceUri& dependency,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || dependency.Empty()) return {};
    Base::Result<Base::String> key =
        MakeKey(dependency, *allocator_);
    if (!key) return key.GetStatus();
    const Impl::Node* node = impl_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.TryReserve(node->dependents.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependentKey : node->dependents) {
        const Impl::Node* dependent = impl_->nodes.Find(dependentKey);
        if (dependent == nullptr) continue;
        Base::Result<void> pushed = output.TryPushBack(dependent->uri);
        if (!pushed) return pushed.GetStatus();
    }
    return {};
}

Base::Result<void> DependencyGraph::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    output.Clear();
    if (impl_ == nullptr || changed.Empty()) return {};

    Base::Vector<Base::String> queue(allocator_);
    Base::HashSet<Base::String> visited(allocator_);
    Base::Result<Base::String> changedKey =
        MakeKey(changed, *allocator_);
    if (!changedKey) return changedKey.GetStatus();
    Base::Result<void> queued =
        queue.TryPushBack(changedKey.Value());
    if (!queued) return queued.GetStatus();

    std::uint32_t cursor = 0U;
    while (cursor < queue.Size()) {
        Base::String key = queue[cursor++];
        Base::Result<typename Base::HashSet<Base::String>::InsertResult>
            inserted = visited.TryInsert(key);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) continue;

        const Impl::Node* node = impl_->nodes.Find(key);
        Base::ResourceUri uri = node != nullptr
            ? node->uri
            : changed;
        Base::Result<void> appended = output.TryPushBack(uri);
        if (!appended) return appended.GetStatus();
        if (node == nullptr) continue;
        for (const Base::String& dependent : node->dependents) {
            if (visited.Contains(dependent)) continue;
            Base::Result<void> next = queue.TryPushBack(dependent);
            if (!next) return next.GetStatus();
        }
    }
    return {};
}

std::uint32_t DependencyGraph::NodeCount() const noexcept {
    return impl_ != nullptr ? impl_->nodes.Size() : 0U;
}

std::uint64_t DependencyGraph::Generation() const noexcept {
    return impl_ != nullptr ? impl_->generation : 0U;
}

struct DocumentCache::Impl final {
    struct Entry final {
        explicit Entry(Base::IAllocator& allocator) noexcept
            : compiledBytes(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> compiledBytes;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    Impl(
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
        compiledBytes -= entry->compiledBytes.Size();
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
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_, limits);
}

DocumentCache::~DocumentCache() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<DocumentCacheLookup> DocumentCache::Lookup(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const Core::MetadataDomain& domain,
    const CompiledDocumentLimits& limits) noexcept {
    DocumentCacheLookup result;
    if (impl_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    Impl::Entry* entry = impl_->entries.Find(key.Value());
    if (entry == nullptr) {
        ++impl_->misses;
        return result;
    }
    if (entry->sourceRevision != sourceRevision ||
        entry->sourceIdentity != sourceIdentity) {
        ++impl_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }

    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            entry->compiledBytes.AsSpan(), domain, limits);
    if (!document) {
        ++impl_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }
    entry->lastAccess = ++impl_->accessSequence;
    ++impl_->hits;
    result.hit = true;
    result.sourceRevision = entry->sourceRevision;
    result.document = std::move(document).Value();
    return result;
}

Base::Result<void> DocumentCache::Store(
    const Base::ResourceUri& uri,
    std::uint64_t sourceRevision,
    std::uint64_t sourceIdentity,
    const CompiledDocument& document,
    Base::Span<const Base::ResourceUri> dependencies) noexcept {
    if (impl_ == nullptr || uri.Empty() || !document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML document cache entry is invalid");
    }
    Base::Result<Base::Vector<std::uint8_t>> serialized =
        document.Serialize();
    if (!serialized) return serialized.GetStatus();
    const std::uint64_t serializedSize =
        serialized.Value().Size();

    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    Impl::Entry* existing = impl_->entries.Find(key.Value());
    if (existing != nullptr) {
        impl_->compiledBytes -= existing->compiledBytes.Size();
        existing->uri = uri;
        existing->compiledBytes = std::move(serialized).Value();
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++impl_->accessSequence;
        impl_->compiledBytes += existing->compiledBytes.Size();
    } else {
        Impl::Entry entry(*allocator_);
        entry.uri = uri;
        entry.compiledBytes = std::move(serialized).Value();
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++impl_->accessSequence;
        impl_->compiledBytes += entry.compiledBytes.Size();
        Base::Result<typename Base::HashMap<Base::String, Impl::Entry>::InsertResult>
            inserted = impl_->entries.TryInsert(
                std::move(key).Value(), std::move(entry));
        if (!inserted) {
            impl_->compiledBytes -= serializedSize;
            return inserted.GetStatus();
        }
    }
    Base::Result<void> graph =
        impl_->graph.Update(uri, dependencies);
    if (!graph) {
        impl_->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++impl_->stores;
    ++impl_->generation;
    impl_->EvictToLimits();
    return {};
}

Base::Result<std::uint32_t> DocumentCache::Invalidate(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (impl_ == nullptr || uri.Empty()) return 0U;
    Base::Vector<Base::ResourceUri> affected(allocator_);
    if (includeDependents) {
        Base::Result<void> collected =
            impl_->graph.CollectAffected(uri, affected);
        if (!collected) return collected.GetStatus();
    } else {
        Base::Result<void> pushed = affected.TryPushBack(uri);
        if (!pushed) return pushed.GetStatus();
    }
    std::uint32_t count = 0U;
    for (std::uint32_t index = affected.Size();
         index > 0U;
         --index) {
        const Base::ResourceUri& affectedUri = affected[index - 1U];
        if (impl_->EraseEntry(affectedUri, false)) {
            ++count;
        } else {
            static_cast<void>(impl_->graph.Remove(affectedUri));
        }
    }
    return count;
}

void DocumentCache::Clear() noexcept {
    if (impl_ == nullptr) return;
    impl_->entries.Clear();
    impl_->graph.Clear();
    impl_->compiledBytes = 0U;
    ++impl_->generation;
}

bool DocumentCache::Contains(
    const Base::ResourceUri& uri) const noexcept {
    if (impl_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    return key && impl_->entries.Contains(key.Value());
}

bool DocumentCache::TryGetSourceRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) const noexcept {
    revision = 0U;
    if (impl_ == nullptr || uri.Empty()) return false;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return false;
    const Impl::Entry* entry = impl_->entries.Find(key.Value());
    if (entry == nullptr || entry->sourceIdentity != sourceIdentity) {
        return false;
    }
    revision = entry->sourceRevision;
    return true;
}

Base::Result<void> DocumentCache::CollectAffected(
    const Base::ResourceUri& changed,
    Base::Vector<Base::ResourceUri>& output) const noexcept {
    return impl_ != nullptr
        ? impl_->graph.CollectAffected(changed, output)
        : Base::Result<void>{};
}

const DependencyGraph& DocumentCache::Dependencies() const noexcept {
    static const DependencyGraph empty;
    return impl_ != nullptr ? impl_->graph : empty;
}

DocumentCacheStatistics DocumentCache::Statistics() const noexcept {
    DocumentCacheStatistics result;
    if (impl_ == nullptr) return result;
    result.entryCount = impl_->entries.Size();
    result.compiledBytes = impl_->compiledBytes;
    result.hitCount = impl_->hits;
    result.missCount = impl_->misses;
    result.storeCount = impl_->stores;
    result.invalidationCount = impl_->invalidations;
    result.evictionCount = impl_->evictions;
    result.generation = impl_->generation;
    return result;
}

const DocumentCacheLimits& DocumentCache::Limits() const noexcept {
    static const DocumentCacheLimits empty{};
    return impl_ != nullptr ? impl_->limits : empty;
}

} // namespace Aero::Markup

// ===== DocumentCache =====



#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>

#include <new>


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

struct DependencyGraphState {
    struct Node {
        explicit Node(Base::IAllocator& allocator) noexcept
            : dependencies(&allocator), dependents(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<Base::String> dependencies;
        Base::Vector<Base::String> dependents;
    };

    explicit DependencyGraphState(Base::IAllocator& allocator) noexcept
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
            inserted = nodes.Insert(
                std::move(key).Value(), std::move(node));
        if (!inserted) return inserted.GetStatus();
        return &inserted.Value().entry->Value();
    }

    Base::IAllocator* allocator = nullptr;
    Base::HashMap<Base::String, Node> nodes;
    std::uint64_t generation = 0U;
};

static_assert(
    sizeof(DependencyGraphState) <= 2048,
    "DependencyGraph inline state storage is too small");
static_assert(
    alignof(DependencyGraphState) <= alignof(std::max_align_t),
    "DependencyGraph inline state alignment is insufficient");

DependencyGraph::DependencyGraph(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) DependencyGraphState(*allocator_);
}

DependencyGraph::~DependencyGraph() noexcept {
    if (state_ == nullptr) return;
    state_->~DependencyGraphState();
    state_ = nullptr;
}

DependencyGraph::DependencyGraph(
    DependencyGraph&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DependencyGraphState(std::move(*other.state_));
        other.state_->~DependencyGraphState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
}

DependencyGraph& DependencyGraph::operator=(
    DependencyGraph&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~DependencyGraphState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DependencyGraphState(std::move(*other.state_));
        other.state_->~DependencyGraphState();
        other.state_ = nullptr;
    }
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
    Base::Result<DependencyGraphState::Node*> documentNode =
        state_->EnsureNode(document);
    if (!documentNode) return documentNode.GetStatus();

    Base::Vector<Base::String> newDependencies(allocator_);
    Base::Result<void> reserved =
        newDependencies.Reserve(dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::ResourceUri& dependencyUri : dependencies) {
        if (dependencyUri.Empty() || dependencyUri == document) continue;
        Base::Result<Base::String> dependencyKey =
            MakeKey(dependencyUri, *allocator_);
        if (!dependencyKey) return dependencyKey.GetStatus();
        if (ContainsKey(newDependencies, dependencyKey.Value().View())) {
            continue;
        }
        Base::Result<DependencyGraphState::Node*> dependencyNode =
            state_->EnsureNode(dependencyUri);
        if (!dependencyNode) return dependencyNode.GetStatus();
        Base::Result<void> appended = newDependencies.PushBack(
            std::move(dependencyKey).Value());
        if (!appended) return appended.GetStatus();
    }

    // Prepare reverse-edge key ownership and vector capacity before mutating
    // any edge. No hash-map insertions occur after this point, so node
    // references remain stable even when EnsureNode() previously rehashed.
    Base::Vector<Base::String> reverseKeys(allocator_);
    Base::Result<void> reverseReserved =
        reverseKeys.Reserve(newDependencies.Size());
    if (!reverseReserved) return reverseReserved.GetStatus();
    for (const Base::String& dependencyKey : newDependencies) {
        DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML dependency graph lost a prepared node");
        }
        Base::Result<void> reverseCapacity =
            dependency->dependents.Reserve(
                dependency->dependents.Size() + 1U);
        if (!reverseCapacity) return reverseCapacity.GetStatus();
        Base::String reverseKey(allocator_);
        Base::Result<void> copied = reverseKey.Assign(
            documentKey.Value().View());
        if (!copied) return copied.GetStatus();
        Base::Result<void> stored = reverseKeys.PushBack(
            std::move(reverseKey));
        if (!stored) return stored.GetStatus();
    }

    DependencyGraphState::Node* node = state_->nodes.Find(documentKey.Value());
    if (node == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML dependency graph lost the document node");
    }
    for (const Base::String& oldDependency : node->dependencies) {
        DependencyGraphState::Node* dependency = state_->nodes.Find(oldDependency);
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
        DependencyGraphState::Node* dependency =
            state_->nodes.Find(node->dependencies[index]);
        if (dependency == nullptr) continue;
        if (ContainsKey(
                dependency->dependents,
                documentKey.Value().View())) {
            continue;
        }
        Base::Result<void> reverse =
            dependency->dependents.PushBack(
                std::move(reverseKeys[index]));
        if (!reverse) return reverse.GetStatus();
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
    DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return false;

    Base::Vector<Base::String> previousDependencies(allocator_);
    if (!previousDependencies.Append(
            node->dependencies.AsSpan())) {
        return false;
    }
    node->dependencies.Clear();
    for (const Base::String& dependencyKey : previousDependencies) {
        DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
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
    const DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependencies.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependencyKey : node->dependencies) {
        const DependencyGraphState::Node* dependency = state_->nodes.Find(dependencyKey);
        if (dependency == nullptr) continue;
        Base::Result<void> pushed = output.PushBack(dependency->uri);
        if (!pushed) return pushed.GetStatus();
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
    const DependencyGraphState::Node* node = state_->nodes.Find(key.Value());
    if (node == nullptr) return {};
    Base::Result<void> reserved =
        output.Reserve(node->dependents.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::String& dependentKey : node->dependents) {
        const DependencyGraphState::Node* dependent = state_->nodes.Find(dependentKey);
        if (dependent == nullptr) continue;
        Base::Result<void> pushed = output.PushBack(dependent->uri);
        if (!pushed) return pushed.GetStatus();
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
    Base::Result<void> queued =
        queue.PushBack(changedKey.Value());
    if (!queued) return queued.GetStatus();

    std::uint32_t cursor = 0U;
    while (cursor < queue.Size()) {
        Base::String key = queue[cursor++];
        Base::Result<typename Base::HashSet<Base::String>::InsertResult>
            inserted = visited.Insert(key);
        if (!inserted) return inserted.GetStatus();
        if (!inserted.Value().inserted) continue;

        const DependencyGraphState::Node* node = state_->nodes.Find(key);
        Base::ResourceUri uri = node != nullptr
            ? node->uri
            : changed;
        Base::Result<void> appended = output.PushBack(uri);
        if (!appended) return appended.GetStatus();
        if (node == nullptr) continue;
        for (const Base::String& dependent : node->dependents) {
            if (visited.Contains(dependent)) continue;
            Base::Result<void> next = queue.PushBack(dependent);
            if (!next) return next.GetStatus();
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

struct DocumentCacheState {
    struct Entry {
        explicit Entry(Base::IAllocator& allocator) noexcept
            : compiledBytes(&allocator) {}

        Base::ResourceUri uri;
        Base::Vector<std::uint8_t> compiledBytes;
        std::uint64_t sourceRevision = 0U;
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t lastAccess = 0U;
    };

    DocumentCacheState(
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

static_assert(
    sizeof(DocumentCacheState) <= 8192,
    "DocumentCache inline state storage is too small");
static_assert(
    alignof(DocumentCacheState) <= alignof(std::max_align_t),
    "DocumentCache inline state alignment is insufficient");

DocumentCache::DocumentCache(
    Base::IAllocator* allocator,
    const DocumentCacheLimits& limits) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_)
        DocumentCacheState(*allocator_, limits);
}

DocumentCache::~DocumentCache() noexcept {
    if (state_ == nullptr) return;
    state_->~DocumentCacheState();
    state_ = nullptr;
}

DocumentCache::DocumentCache(
    DocumentCache&& other) noexcept
    : allocator_(other.allocator_) {
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DocumentCacheState(std::move(*other.state_));
        other.state_->~DocumentCacheState();
        other.state_ = nullptr;
    }
    other.allocator_ = nullptr;
}

DocumentCache& DocumentCache::operator=(
    DocumentCache&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        state_->~DocumentCacheState();
        state_ = nullptr;
    }
    allocator_ = other.allocator_;
    if (other.state_ != nullptr) {
        state_ = new (stateStorage_)
            DocumentCacheState(std::move(*other.state_));
        other.state_->~DocumentCacheState();
        other.state_ = nullptr;
    }
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
    if (state_ == nullptr || uri.Empty()) return result;
    Base::Result<Base::String> key = MakeKey(uri, *allocator_);
    if (!key) return key.GetStatus();
    DocumentCacheState::Entry* entry = state_->entries.Find(key.Value());
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

    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
            entry->compiledBytes.AsSpan(), domain, limits);
    if (!document) {
        ++state_->misses;
        Base::Result<std::uint32_t> invalidated =
            Invalidate(uri, true);
        if (!invalidated) return invalidated.GetStatus();
        return result;
    }
    entry->lastAccess = ++state_->accessSequence;
    ++state_->hits;
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
    if (state_ == nullptr || uri.Empty() || !document.IsValid()) {
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
    DocumentCacheState::Entry* existing = state_->entries.Find(key.Value());
    if (existing != nullptr) {
        state_->compiledBytes -= existing->compiledBytes.Size();
        existing->uri = uri;
        existing->compiledBytes = std::move(serialized).Value();
        existing->sourceRevision = sourceRevision;
        existing->sourceIdentity = sourceIdentity;
        existing->lastAccess = ++state_->accessSequence;
        state_->compiledBytes += existing->compiledBytes.Size();
    } else {
        DocumentCacheState::Entry entry(*allocator_);
        entry.uri = uri;
        entry.compiledBytes = std::move(serialized).Value();
        entry.sourceRevision = sourceRevision;
        entry.sourceIdentity = sourceIdentity;
        entry.lastAccess = ++state_->accessSequence;
        state_->compiledBytes += entry.compiledBytes.Size();
        Base::Result<typename Base::HashMap<Base::String, DocumentCacheState::Entry>::InsertResult>
            inserted = state_->entries.Insert(
                std::move(key).Value(), std::move(entry));
        if (!inserted) {
            state_->compiledBytes -= serializedSize;
            return inserted.GetStatus();
        }
    }
    Base::Result<void> graph =
        state_->graph.Update(uri, dependencies);
    if (!graph) {
        state_->EraseEntry(uri, false);
        return graph.GetStatus();
    }
    ++state_->stores;
    ++state_->generation;
    state_->EvictToLimits();
    return {};
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
        Base::Result<void> pushed = affected.PushBack(uri);
        if (!pushed) return pushed.GetStatus();
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
    const DocumentCacheState::Entry* entry = state_->entries.Find(key.Value());
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



#pragma once

// P2.4: Dispatcher-owned slab for dependency-property storage blocks.
//
// Replaces the former process-global thread_local pools
// (g_storedValueRarePool / g_propertyStorePool): pooled blocks now live and
// die with the owning Dispatcher instead of lingering per thread forever,
// and cross-thread frees route back to the owning slab instead of migrating
// between thread-local caches.
//
// Design notes:
// - Exactly two fixed size classes (StoredValueEntry rare blocks and
//   PropertyStore blocks), each with its own capped free list (256).
// - Every allocation prepends a 16-byte BlockHeader carrying the owning
//   slab (or nullptr for plain-heap fallback blocks). Release therefore
//   needs no calling context and is safe from destructors on any thread.
// - 16-byte header keeps the payload 16-aligned (Value storage requires it).
// - Thread-safe via a short mutex; steady-state hit rate makes contention
//   negligible, and all mutation fast paths already serialize on the
//   dispatcher thread.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <new>

namespace Aero {

class PropertySlab {
private:
    struct FreeNode {
        FreeNode* next;
    };

    struct SlabState {
        mutable std::mutex mutex;
        FreeNode* rareFree = nullptr;
        FreeNode* storeFree = nullptr;
        std::size_t rareCount = 0U;
        std::size_t storeCount = 0U;
        std::uint32_t outstanding = 0U;
        bool alive = true;
    };

public:
    static constexpr std::size_t kMaxPooledPerClass = 256U;

    struct BlockHeader {
        SlabState* owner;  // nullptr == plain-heap fallback block
        std::uint64_t reserved = 0U;
    };

    static_assert(sizeof(BlockHeader) == 16U, "PropertySlab header must preserve 16-byte payload alignment");

    PropertySlab() noexcept : state_(new (std::nothrow) SlabState()) {}

    ~PropertySlab() noexcept {
        if (state_ == nullptr) { return; }
        bool deleteNow = false;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            DrainList(state_->rareFree);
            state_->rareCount = 0U;
            DrainList(state_->storeFree);
            state_->storeCount = 0U;
            state_->alive = false;
            deleteNow = (state_->outstanding == 0U);
        }
        if (deleteNow) {
            delete state_;
        }
        state_ = nullptr;
    }

    PropertySlab(const PropertySlab&) = delete;
    PropertySlab& operator=(const PropertySlab&) = delete;
    PropertySlab(PropertySlab&&) = delete;
    PropertySlab& operator=(PropertySlab&&) = delete;

    void* AllocateRare(std::size_t size) noexcept {
        if (state_ == nullptr) { return AllocateHeap(size); }
        return Allocate(size, state_->rareFree, state_->rareCount);
    }

    static void ReleaseRare(void* payload) noexcept { Release(payload, &PropertySlab::ReleaseRareToOwner); }

    void* AllocateStore(std::size_t size) noexcept {
        if (state_ == nullptr) { return AllocateHeap(size); }
        return Allocate(size, state_->storeFree, state_->storeCount);
    }

    static void ReleaseStore(void* payload) noexcept { Release(payload, &PropertySlab::ReleaseStoreToOwner); }

    // Live (not yet released) slab-owned blocks. Diagnostic only; no
    // enforcement, since leaked host objects may outlive teardown.
    std::uint32_t OutstandingBlocks() const noexcept {
        if (state_ == nullptr) { return 0U; }
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->outstanding;
    }

    // Drops pooled (recycled) blocks. Live blocks are untouched; they belong
    // to live DependencyObjects, which must already be gone when the owning
    // Dispatcher is destroyed (DispatcherObject holds a Dispatcher&).
    void Clear() noexcept {
        if (state_ == nullptr) { return; }
        std::lock_guard<std::mutex> lock(state_->mutex);
        DrainList(state_->rareFree);
        state_->rareCount = 0U;
        DrainList(state_->storeFree);
        state_->storeCount = 0U;
    }

    // Plain-heap fallback: used by context-free copies (e.g. entry copy on
    // HashMap growth) that have no owning slab at hand. Freed through the
    // same class operator delete via the null-owner header.
    static void* AllocateHeap(std::size_t size) noexcept {
        void* chunk = std::malloc(sizeof(BlockHeader) + size);
        if (chunk == nullptr) { return nullptr; }
        auto* header = static_cast<BlockHeader*>(chunk);
        header->owner = nullptr;
        header->reserved = 0U;
        return HeaderToPayload(header);
    }

private:
    SlabState* state_ = nullptr;

    void* Allocate(std::size_t size, FreeNode*& freeList, std::size_t& pooled) noexcept {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (freeList != nullptr) {
            // Free-list nodes ARE payload pointers (their headers stay
            // attached in front); return as-is, do not re-add the offset.
            FreeNode* node = freeList;
            freeList = node->next;
            --pooled;
            ++state_->outstanding;
            return node;
        }
        void* chunk = std::malloc(sizeof(BlockHeader) + size);
        if (chunk == nullptr) { return nullptr; }
        auto* header = static_cast<BlockHeader*>(chunk);
        header->owner = state_;
        header->reserved = 0U;
        ++state_->outstanding;
        return HeaderToPayload(header);
    }

    using ReleaseToOwner = void (*)(SlabState*, void*) noexcept;

    static void Release(void* payload, ReleaseToOwner toOwner) noexcept {
        if (payload == nullptr) { return; }
        BlockHeader* header = PayloadToHeader(payload);
        SlabState* owner = header->owner;
        if (owner == nullptr) {
            std::free(header);
            return;
        }
        toOwner(owner, payload);
    }

    static void ReleaseRareToOwner(SlabState* owner, void* payload) noexcept {
        bool deleteOwner = false;
        {
            std::lock_guard<std::mutex> lock(owner->mutex);
            if (owner->outstanding != 0U) { --owner->outstanding; }
            if (owner->alive && owner->rareCount < kMaxPooledPerClass) {
                auto* node = static_cast<FreeNode*>(payload);
                node->next = owner->rareFree;
                owner->rareFree = node;
                ++owner->rareCount;
            } else {
                std::free(PayloadToHeader(payload));
                deleteOwner = (!owner->alive && owner->outstanding == 0U);
            }
        }
        if (deleteOwner) {
            delete owner;
        }
    }

    static void ReleaseStoreToOwner(SlabState* owner, void* payload) noexcept {
        bool deleteOwner = false;
        {
            std::lock_guard<std::mutex> lock(owner->mutex);
            if (owner->outstanding != 0U) { --owner->outstanding; }
            if (owner->alive && owner->storeCount < kMaxPooledPerClass) {
                auto* node = static_cast<FreeNode*>(payload);
                node->next = owner->storeFree;
                owner->storeFree = node;
                ++owner->storeCount;
            } else {
                std::free(PayloadToHeader(payload));
                deleteOwner = (!owner->alive && owner->outstanding == 0U);
            }
        }
        if (deleteOwner) {
            delete owner;
        }
    }

    static void* HeaderToPayload(void* chunk) noexcept {
        return static_cast<void*>(static_cast<unsigned char*>(chunk) + sizeof(BlockHeader));
    }

    static BlockHeader* PayloadToHeader(void* payload) noexcept {
        return reinterpret_cast<BlockHeader*>(static_cast<unsigned char*>(payload) - sizeof(BlockHeader));
    }

    static void DrainList(FreeNode*& freeList) noexcept {
        FreeNode* cursor = freeList;
        while (cursor != nullptr) {
            FreeNode* next = cursor->next;
            std::free(PayloadToHeader(cursor));
            cursor = next;
        }
        freeList = nullptr;
    }
};

} // namespace Aero

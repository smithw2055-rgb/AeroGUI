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

namespace Aero {

class PropertySlab {
public:
    static constexpr std::size_t kMaxPooledPerClass = 256U;

    struct BlockHeader {
        PropertySlab* owner;  // nullptr == plain-heap fallback block
        std::uint64_t reserved = 0U;
    };

    static_assert(
        sizeof(BlockHeader) == 16U,
        "PropertySlab header must preserve 16-byte payload alignment");

    PropertySlab() noexcept = default;
    ~PropertySlab() noexcept {
        Clear();
    }

    PropertySlab(const PropertySlab&) = delete;
    PropertySlab& operator=(const PropertySlab&) = delete;
    PropertySlab(PropertySlab&&) = delete;
    PropertySlab& operator=(PropertySlab&&) = delete;

    void* AllocateRare(std::size_t size) noexcept {
        return Allocate(size, rareFree_, rareCount_);
    }

    static void ReleaseRare(void* payload) noexcept {
        Release(payload, &PropertySlab::ReleaseRareToOwner);
    }

    void* AllocateStore(std::size_t size) noexcept {
        return Allocate(size, storeFree_, storeCount_);
    }

    static void ReleaseStore(void* payload) noexcept {
        Release(payload, &PropertySlab::ReleaseStoreToOwner);
    }

    // Live (not yet released) slab-owned blocks. Diagnostic only; no
    // enforcement, since leaked host objects may outlive teardown.
    std::uint32_t OutstandingBlocks() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return outstanding_;
    }

    // Drops pooled (recycled) blocks. Live blocks are untouched; they belong
    // to live DependencyObjects, which must already be gone when the owning
    // Dispatcher is destroyed (DispatcherObject holds a Dispatcher&).
    void Clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        DrainList(rareFree_);
        rareCount_ = 0U;
        DrainList(storeFree_);
        storeCount_ = 0U;
    }

    // Plain-heap fallback: used by context-free copies (e.g. entry copy on
    // HashMap growth) that have no owning slab at hand. Freed through the
    // same class operator delete via the null-owner header.
    static void* AllocateHeap(std::size_t size) noexcept {
        void* chunk = std::malloc(sizeof(BlockHeader) + size);
        if (chunk == nullptr) {
            return nullptr;
        }
        auto* header = static_cast<BlockHeader*>(chunk);
        header->owner = nullptr;
        header->reserved = 0U;
        return HeaderToPayload(header);
    }

private:
    struct FreeNode {
        FreeNode* next;
    };

    mutable std::mutex mutex_;
    FreeNode* rareFree_ = nullptr;
    FreeNode* storeFree_ = nullptr;
    std::size_t rareCount_ = 0U;
    std::size_t storeCount_ = 0U;
    std::uint32_t outstanding_ = 0U;

    void* Allocate(
        std::size_t size,
        FreeNode*& freeList,
        std::size_t& pooled) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (freeList != nullptr) {
            // Free-list nodes ARE payload pointers (their headers stay
            // attached in front); return as-is, do not re-add the offset.
            FreeNode* node = freeList;
            freeList = node->next;
            --pooled;
            ++outstanding_;
            return node;
        }
        void* chunk = std::malloc(sizeof(BlockHeader) + size);
        if (chunk == nullptr) {
            return nullptr;
        }
        auto* header = static_cast<BlockHeader*>(chunk);
        header->owner = this;
        header->reserved = 0U;
        ++outstanding_;
        return HeaderToPayload(header);
    }

    using ReleaseToOwner = void (PropertySlab::*)(void*) noexcept;

    static void Release(void* payload, ReleaseToOwner toOwner) noexcept {
        if (payload == nullptr) {
            return;
        }
        BlockHeader* header = PayloadToHeader(payload);
        PropertySlab* owner = header->owner;
        if (owner == nullptr) {
            std::free(header);
            return;
        }
        (owner->*toOwner)(payload);
    }

    void ReleaseRareToOwner(void* payload) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outstanding_ != 0U) {
            --outstanding_;
        }
        if (rareCount_ < kMaxPooledPerClass) {
            auto* node = static_cast<FreeNode*>(payload);
            node->next = rareFree_;
            rareFree_ = node;
            ++rareCount_;
        } else {
            std::free(PayloadToHeader(payload));
        }
    }

    void ReleaseStoreToOwner(void* payload) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (outstanding_ != 0U) {
            --outstanding_;
        }
        if (storeCount_ < kMaxPooledPerClass) {
            auto* node = static_cast<FreeNode*>(payload);
            node->next = storeFree_;
            storeFree_ = node;
            ++storeCount_;
        } else {
            std::free(PayloadToHeader(payload));
        }
    }

    static void* HeaderToPayload(void* chunk) noexcept {
        return static_cast<void*>(
            static_cast<unsigned char*>(chunk) + sizeof(BlockHeader));
    }

    static BlockHeader* PayloadToHeader(void* payload) noexcept {
        return reinterpret_cast<BlockHeader*>(
            static_cast<unsigned char*>(payload) - sizeof(BlockHeader));
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

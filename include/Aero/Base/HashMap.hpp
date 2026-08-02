#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

template<class K, class V, class Hash = DefaultHash<K>,
    class Equal = DefaultEqual<K>>
class HashMap  {
public:
    using KeyType = K;
    using MappedType = V;
    using SizeType = std::uint32_t;

    class Entry  {
    public:
        const K& Key() const noexcept { return key_; }
        V& Value() noexcept { return value_; }
        const V& Value() const noexcept { return value_; }

    private:
        friend class HashMap;

        template<class KeyArg, class ValueArg>
        Entry(HashCode hash, KeyArg&& key, ValueArg&& value)
            : hash_(hash),
              key_(std::forward<KeyArg>(key)),
              value_(std::forward<ValueArg>(value)) {}

        HashCode hash_ = 0U;
        K key_;
        V value_;
    };

    struct InsertResult  {
        Entry* entry = nullptr;
        bool inserted = false;
    };

private:
    enum class BucketState : std::uint8_t {
        Empty,
        Occupied,
        Tombstone
    };

    struct Bucket  {
        BucketState state = BucketState::Empty;
        alignas(Entry) unsigned char storage[sizeof(Entry)];

        Entry* GetEntry() noexcept {
            return std::launder(reinterpret_cast<Entry*>(storage));
        }

        const Entry* GetEntry() const noexcept {
            return std::launder(reinterpret_cast<const Entry*>(storage));
        }
    };

public:
    template<bool IsConst>
    class IteratorBase  {
    private:
        using MapType = typename std::conditional<IsConst,
            const HashMap, HashMap>::type;
        using EntryType = typename std::conditional<IsConst,
            const Entry, Entry>::type;

    public:
        IteratorBase() noexcept = default;

        EntryType& operator*() const noexcept {
            AERO_ASSERT(map_ != nullptr && index_ < map_->capacity_);
            return *map_->buckets_[index_].GetEntry();
        }

        EntryType* operator->() const noexcept {
            return &(**this);
        }

        IteratorBase& operator++() noexcept {
            AERO_ASSERT(map_ != nullptr && index_ <= map_->capacity_);
            if (index_ < map_->capacity_) {
                ++index_;
                SkipEmpty();
            }
            return *this;
        }

        bool operator==(const IteratorBase& other) const noexcept {
            return map_ == other.map_ && index_ == other.index_;
        }

        bool operator!=(const IteratorBase& other) const noexcept {
            return !(*this == other);
        }

    private:
        friend class HashMap;

        IteratorBase(MapType* map, SizeType index) noexcept
            : map_(map), index_(index) {
            SkipEmpty();
        }

        void SkipEmpty() noexcept {
            while (map_ != nullptr && index_ < map_->capacity_ &&
                map_->buckets_[index_].state != BucketState::Occupied) {
                ++index_;
            }
        }

        MapType* map_ = nullptr;
        SizeType index_ = 0U;
    };

    using Iterator = IteratorBase<false>;
    using ConstIterator = IteratorBase<true>;

    explicit HashMap(
        IAllocator* allocator = nullptr,
        HashCode seed = 0U,
        Hash hash = Hash{},
        Equal equal = Equal{}) noexcept
        : allocator_(allocator != nullptr ? allocator : &GetDefaultAllocator()),
          hash_(std::move(hash)),
          equal_(std::move(equal)),
          seed_(seed) {}

    HashMap(const HashMap& other)
        : allocator_(&other.Allocator()),
          hash_(other.hash_),
          equal_(other.equal_),
          seed_(other.seed_) {
        CopyFromOrAbort(other);
    }

    HashMap(HashMap&& other) noexcept
        : allocator_(other.allocator_),
          buckets_(other.buckets_),
          size_(other.size_),
          used_(other.used_),
          capacity_(other.capacity_),
          hash_(std::move(other.hash_)),
          equal_(std::move(other.equal_)),
          seed_(other.seed_) {
        other.buckets_ = nullptr;
        other.size_ = 0U;
        other.used_ = 0U;
        other.capacity_ = 0U;
    }

    ~HashMap() {
        DestroyAndReleaseBuckets();
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            HashMap temporary(allocator_, other.seed_, other.hash_, other.equal_);
            temporary.CopyFromOrAbort(other);
            AdoptStorageFrom(temporary);
        }
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            if (allocator_ == other.allocator_) {
                DestroyAndReleaseBuckets();
                buckets_ = other.buckets_;
                size_ = other.size_;
                used_ = other.used_;
                capacity_ = other.capacity_;
                hash_ = std::move(other.hash_);
                equal_ = std::move(other.equal_);
                seed_ = other.seed_;
                other.buckets_ = nullptr;
                other.size_ = 0U;
                other.used_ = 0U;
                other.capacity_ = 0U;
            } else {
                HashMap temporary(allocator_, other.seed_, other.hash_, other.equal_);
                const Result<void> reserveResult = temporary.Reserve(other.size_);
                if (!reserveResult) {
                    ReportOutOfMemory(BucketBytesForCapacity(
                        RequiredCapacityFor(other.size_)), alignof(Bucket),
                        MemoryTag::Container);
                }
                for (SizeType index = 0U; index < other.capacity_; ++index) {
                    Bucket& bucket = other.buckets_[index];
                    if (bucket.state == BucketState::Occupied) {
                        Entry* entry = bucket.GetEntry();
                        const Result<InsertResult> insertResult = temporary.Insert(
                            std::move_if_noexcept(entry->key_),
                            std::move_if_noexcept(entry->value_));
                        if (!insertResult) {
                            ReportOutOfMemory(BucketBytesForCapacity(
                                RequiredCapacityFor(other.size_)), alignof(Bucket),
                                MemoryTag::Container);
                        }
                    }
                }
                other.DestroyAndReleaseBuckets();
                AdoptStorageFrom(temporary);
            }
        }
        return *this;
    }

    SizeType Size() const noexcept { return size_; }
    SizeType Capacity() const noexcept { return capacity_; }
    bool Empty() const noexcept { return size_ == 0U; }
    HashCode Seed() const noexcept { return seed_; }
    IAllocator& Allocator() const noexcept { return *allocator_; }

    Iterator begin() noexcept { return Iterator(this, 0U); }
    Iterator end() noexcept { return Iterator(this, capacity_); }
    ConstIterator begin() const noexcept {
        return ConstIterator(this, 0U);
    }
    ConstIterator end() const noexcept {
        return ConstIterator(this, capacity_);
    }

    void Clear() noexcept {
        if (buckets_ == nullptr) {
            return;
        }
        for (SizeType index = 0U; index < capacity_; ++index) {
            Bucket& bucket = buckets_[index];
            if (bucket.state == BucketState::Occupied) {
                bucket.GetEntry()->~Entry();
            }
            bucket.state = BucketState::Empty;
        }
        size_ = 0U;
        used_ = 0U;
    }

    Result<void> Reserve(SizeType expectedElements) noexcept {
        const SizeType requiredCapacity = RequiredCapacityFor(expectedElements);
        if (expectedElements > 0U && requiredCapacity == 0U) {
            return Status::Failure(ErrorCode::OutOfRange,
                "HashMap requested size exceeds supported capacity");
        }
        if (requiredCapacity <= capacity_) {
            return {};
        }
        return Rehash(requiredCapacity);
    }

    V* Find(const K& key) noexcept {
        Entry* entry = FindEntry(key);
        return entry != nullptr ? &entry->value_ : nullptr;
    }

    const V* Find(const K& key) const noexcept {
        const Entry* entry = FindEntry(key);
        return entry != nullptr ? &entry->value_ : nullptr;
    }

    Entry* FindEntry(const K& key) noexcept {
        return const_cast<Entry*>(
            static_cast<const HashMap*>(this)->FindEntry(key));
    }

    const Entry* FindEntry(const K& key) const noexcept {
        if (capacity_ == 0U) {
            return nullptr;
        }
        const HashCode hash = ComputeHash(key);
        const ProbeResult probe = Probe(key, hash);
        return probe.found ? buckets_[probe.index].GetEntry() : nullptr;
    }

    bool Contains(const K& key) const noexcept {
        return FindEntry(key) != nullptr;
    }

    Result<InsertResult> Insert(
        const K& key, const V& value) noexcept {
        return InsertImpl(key, value);
    }

    Result<InsertResult> Insert(
        K&& key, V&& value) noexcept {
        return InsertImpl(std::move(key), std::move(value));
    }

    Result<V*> Set(
        const K& key, const V& value) noexcept {
        Entry* existing = FindEntry(key);
        if (existing != nullptr) {
            existing->value_ = value;
            return &existing->value_;
        }
        Result<InsertResult> inserted = Insert(key, value);
        if (!inserted) {
            return inserted.GetStatus();
        }
        return &inserted.Value().entry->value_;
    }

    Result<V*> Set(K&& key, V&& value) noexcept {
        Entry* existing = FindEntry(key);
        if (existing != nullptr) {
            existing->value_ = std::move(value);
            return &existing->value_;
        }
        Result<InsertResult> inserted = Insert(
            std::move(key), std::move(value));
        if (!inserted) {
            return inserted.GetStatus();
        }
        return &inserted.Value().entry->value_;
    }

    bool Erase(const K& key) noexcept {
        if (capacity_ == 0U) {
            return false;
        }
        const HashCode hash = ComputeHash(key);
        const ProbeResult probe = Probe(key, hash);
        if (!probe.found) {
            return false;
        }

        Bucket& bucket = buckets_[probe.index];
        bucket.GetEntry()->~Entry();
        bucket.state = BucketState::Tombstone;
        --size_;
        if (size_ == 0U) {
            for (SizeType index = 0U; index < capacity_; ++index) {
                buckets_[index].state = BucketState::Empty;
            }
            used_ = 0U;
        }
        return true;
    }

private:
    static constexpr SizeType MinimumCapacity = 8U;
    static constexpr SizeType MaximumCapacity = SizeType{1U} << 30U;
    static constexpr std::uint64_t LoadNumerator = 7U;
    static constexpr std::uint64_t LoadDenominator = 10U;
    static constexpr SizeType InvalidIndex = UINT32_MAX;

    struct ProbeResult  {
        bool found = false;
        SizeType index = InvalidIndex;
    };

    IAllocator* allocator_ = nullptr;
    Bucket* buckets_ = nullptr;
    SizeType size_ = 0U;
    SizeType used_ = 0U;
    SizeType capacity_ = 0U;
    Hash hash_;
    Equal equal_;
    HashCode seed_ = 0U;

    HashCode ComputeHash(const K& key) const noexcept {
        return MixHash64(hash_(key) ^ seed_);
    }

    template<class KeyArg, class ValueArg>
    Result<InsertResult> InsertImpl(
        KeyArg&& key, ValueArg&& value) noexcept {
        HashCode hash = ComputeHash(key);
        if (capacity_ != 0U) {
            const ProbeResult existing = Probe(key, hash);
            if (existing.found) {
                return InsertResult{buckets_[existing.index].GetEntry(), false};
            }
        }

        const Result<void> capacityResult = EnsureInsertCapacity();
        if (!capacityResult) {
            return capacityResult.GetStatus();
        }

        hash = ComputeHash(key);
        const ProbeResult target = Probe(key, hash);
        AERO_ASSERT(!target.found && target.index != InvalidIndex);
        Bucket& bucket = buckets_[target.index];
        const bool wasEmpty = bucket.state == BucketState::Empty;
        Entry* entry = new (bucket.storage) Entry(
            hash, std::forward<KeyArg>(key), std::forward<ValueArg>(value));
        bucket.state = BucketState::Occupied;
        ++size_;
        if (wasEmpty) {
            ++used_;
        }
        return InsertResult{entry, true};
    }

    Result<void> EnsureInsertCapacity() noexcept {
        if (capacity_ == 0U) {
            return Rehash(MinimumCapacity);
        }

        const std::uint64_t projectedUsed =
            static_cast<std::uint64_t>(used_) + 1U;
        if (projectedUsed * LoadDenominator <=
            static_cast<std::uint64_t>(capacity_) * LoadNumerator) {
            return {};
        }

        const SizeType tombstones = used_ - size_;
        if (tombstones > size_ / 2U) {
            return Rehash(capacity_);
        }

        if (capacity_ >= MaximumCapacity) {
            return Status::Failure(ErrorCode::OutOfRange,
                "HashMap capacity limit reached");
        }
        return Rehash(capacity_ * 2U);
    }

    ProbeResult Probe(
        const K& key, HashCode hash) const noexcept {
        AERO_ASSERT(capacity_ != 0U && (capacity_ & (capacity_ - 1U)) == 0U);
        const SizeType mask = capacity_ - 1U;
        SizeType index = static_cast<SizeType>(hash) & mask;
        SizeType firstTombstone = InvalidIndex;

        for (SizeType attempt = 0U; attempt < capacity_; ++attempt) {
            const Bucket& bucket = buckets_[index];
            if (bucket.state == BucketState::Empty) {
                return {false, firstTombstone != InvalidIndex
                    ? firstTombstone : index};
            }
            if (bucket.state == BucketState::Tombstone) {
                if (firstTombstone == InvalidIndex) {
                    firstTombstone = index;
                }
            } else {
                const Entry* entry = bucket.GetEntry();
                if (entry->hash_ == hash && equal_(entry->key_, key)) {
                    return {true, index};
                }
            }
            index = (index + 1U) & mask;
        }

        return {false, firstTombstone};
    }

    Result<void> Rehash(SizeType newCapacity) noexcept {
        if (newCapacity < MinimumCapacity) {
            newCapacity = MinimumCapacity;
        }
        if (newCapacity > MaximumCapacity ||
            (newCapacity & (newCapacity - 1U)) != 0U) {
            return Status::Failure(ErrorCode::OutOfRange,
                "HashMap capacity must be a supported power of two");
        }

        const std::size_t bytes = BucketBytesForCapacity(newCapacity);
        if (bytes == 0U) {
            return Status::Failure(ErrorCode::OutOfRange,
                "HashMap bucket storage exceeds addressable memory");
        }

        void* memory = allocator_->Allocate(
            {bytes, alignof(Bucket), MemoryTag::Container});
        if (memory == nullptr) {
            return Status::Failure(ErrorCode::OutOfMemory,
                "HashMap bucket allocation failed");
        }

        Bucket* replacement = static_cast<Bucket*>(memory);
        for (SizeType index = 0U; index < newCapacity; ++index) {
            new (replacement + index) Bucket();
        }

        Bucket* oldBuckets = buckets_;
        const SizeType oldCapacity = capacity_;
        buckets_ = replacement;
        capacity_ = newCapacity;
        size_ = 0U;
        used_ = 0U;

        for (SizeType index = 0U; index < oldCapacity; ++index) {
            Bucket& oldBucket = oldBuckets[index];
            if (oldBucket.state == BucketState::Occupied) {
                Entry* oldEntry = oldBucket.GetEntry();
                InsertTransferred(*oldEntry);
                oldEntry->~Entry();
            }
            oldBucket.~Bucket();
        }

        if (oldBuckets != nullptr) {
            allocator_->Deallocate(oldBuckets,
                BucketBytesForCapacity(oldCapacity), alignof(Bucket),
                MemoryTag::Container);
        }
        return {};
    }

    void InsertTransferred(Entry& source) noexcept {
        const ProbeResult target = Probe(source.key_, source.hash_);
        AERO_ASSERT(!target.found && target.index != InvalidIndex);
        Bucket& bucket = buckets_[target.index];
        const bool wasEmpty = bucket.state == BucketState::Empty;
        new (bucket.storage) Entry(source.hash_,
            std::move_if_noexcept(source.key_),
            std::move_if_noexcept(source.value_));
        bucket.state = BucketState::Occupied;
        ++size_;
        if (wasEmpty) {
            ++used_;
        }
    }

    void DestroyAndReleaseBuckets() noexcept {
        if (buckets_ == nullptr) {
            return;
        }
        for (SizeType index = 0U; index < capacity_; ++index) {
            Bucket& bucket = buckets_[index];
            if (bucket.state == BucketState::Occupied) {
                bucket.GetEntry()->~Entry();
            }
            bucket.~Bucket();
        }
        allocator_->Deallocate(buckets_,
            BucketBytesForCapacity(capacity_), alignof(Bucket),
            MemoryTag::Container);
        buckets_ = nullptr;
        size_ = 0U;
        used_ = 0U;
        capacity_ = 0U;
    }

    void CopyFromOrAbort(const HashMap& other) {
        const Result<void> reserveResult = Reserve(other.size_);
        if (!reserveResult) {
            ReportOutOfMemory(BucketBytesForCapacity(
                RequiredCapacityFor(other.size_)), alignof(Bucket),
                MemoryTag::Container);
        }
        for (const Entry& entry : other) {
            const Result<InsertResult> insertResult = Insert(
                entry.key_, entry.value_);
            if (!insertResult) {
                ReportOutOfMemory(BucketBytesForCapacity(
                    RequiredCapacityFor(other.size_)), alignof(Bucket),
                    MemoryTag::Container);
            }
        }
    }

    void AdoptStorageFrom(HashMap& other) noexcept {
        AERO_ASSERT(allocator_ == other.allocator_);
        DestroyAndReleaseBuckets();
        buckets_ = other.buckets_;
        size_ = other.size_;
        used_ = other.used_;
        capacity_ = other.capacity_;
        hash_ = std::move(other.hash_);
        equal_ = std::move(other.equal_);
        seed_ = other.seed_;
        other.buckets_ = nullptr;
        other.size_ = 0U;
        other.used_ = 0U;
        other.capacity_ = 0U;
    }

    static SizeType RequiredCapacityFor(
        SizeType expectedElements) noexcept {
        if (expectedElements == 0U) {
            return 0U;
        }
        SizeType capacity = MinimumCapacity;
        while (static_cast<std::uint64_t>(expectedElements) * LoadDenominator >
            static_cast<std::uint64_t>(capacity) * LoadNumerator) {
            if (capacity >= MaximumCapacity) {
                return 0U;
            }
            capacity *= 2U;
        }
        return capacity;
    }

    static std::size_t BucketBytesForCapacity(
        SizeType capacity) noexcept {
        if (capacity == 0U) {
            return 0U;
        }
        if (static_cast<std::uint64_t>(capacity) >
            static_cast<std::uint64_t>(SIZE_MAX / sizeof(Bucket))) {
            return 0U;
        }
        return static_cast<std::size_t>(capacity) * sizeof(Bucket);
    }
};

} // namespace Aero::Base

#pragma once

#include <Aero/Base/HashMap.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Base {
struct HashSetMarker  {};

template<class T, class Hash = DefaultHash<T>, class Equal = DefaultEqual<T>>
class HashSet  {
private:
    using Map = HashMap<T, HashSetMarker, Hash, Equal>;

public:
    using SizeType = std::uint32_t;

    struct InsertResult  {
        const T* value = nullptr;
        bool inserted = false;
    };

    template<bool IsConst>
    class IteratorBase  {
    private:
        using MapIterator = typename std::conditional<IsConst,
            typename Map::ConstIterator, typename Map::Iterator>::type;

    public:
        IteratorBase() noexcept = default;

        const T& operator*() const noexcept {
            return iterator_->Key();
        }

        const T* operator->() const noexcept {
            return &iterator_->Key();
        }

        IteratorBase& operator++() noexcept {
            ++iterator_;
            return *this;
        }

        bool operator==(const IteratorBase& other) const noexcept {
            return iterator_ == other.iterator_;
        }

        bool operator!=(const IteratorBase& other) const noexcept {
            return !(*this == other);
        }

    private:
        friend class HashSet;
        explicit IteratorBase(MapIterator iterator) noexcept
            : iterator_(iterator) {}

        MapIterator iterator_;
    };

    using Iterator = IteratorBase<false>;
    using ConstIterator = IteratorBase<true>;

    explicit HashSet(
        IAllocator* allocator = nullptr,
        HashCode seed = 0U,
        Hash hash = Hash{},
        Equal equal = Equal{}) noexcept
        : map_(allocator, seed, std::move(hash), std::move(equal)) {}

    HashSet(const HashSet&) = default;
    HashSet(HashSet&&) noexcept = default;
    HashSet& operator=(const HashSet&) = default;
    HashSet& operator=(HashSet&&) noexcept = default;
    ~HashSet() = default;

    SizeType Size() const noexcept { return map_.Size(); }
    SizeType Capacity() const noexcept { return map_.Capacity(); }
    bool Empty() const noexcept { return map_.Empty(); }
    HashCode Seed() const noexcept { return map_.Seed(); }
    IAllocator& Allocator() const noexcept { return map_.Allocator(); }

    Iterator begin() noexcept { return Iterator(map_.begin()); }
    Iterator end() noexcept { return Iterator(map_.end()); }
    ConstIterator begin() const noexcept {
        return ConstIterator(map_.begin());
    }
    ConstIterator end() const noexcept {
        return ConstIterator(map_.end());
    }

    void Clear() noexcept { map_.Clear(); }

    Result<void> Reserve(SizeType expectedElements) noexcept {
        return map_.Reserve(expectedElements);
    }

    bool Contains(const T& value) const noexcept {
        return map_.Contains(value);
    }

    bool Erase(const T& value) noexcept {
        return map_.Erase(value);
    }

    Result<InsertResult> Insert(const T& value) noexcept {
        Result<typename Map::InsertResult> result = map_.Insert(
            value, HashSetMarker{});
        if (!result) {
            return result.GetStatus();
        }
        return InsertResult{&result.Value().entry->Key(), result.Value().inserted};
    }

    Result<InsertResult> Insert(T&& value) noexcept {
        Result<typename Map::InsertResult> result = map_.Insert(
            std::move(value), HashSetMarker{});
        if (!result) {
            return result.GetStatus();
        }
        return InsertResult{&result.Value().entry->Key(), result.Value().inserted};
    }

private:
    Map map_;
};

} // namespace Aero::Base

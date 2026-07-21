#pragma once

#include <Aero/Base/HashMap.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Base {
namespace Detail {
struct HashSetMarker final {};
} // namespace Detail

template<class T, class Hash = DefaultHash<T>, class Equal = DefaultEqual<T>>
class HashSet final {
private:
    using Map = HashMap<T, Detail::HashSetMarker, Hash, Equal>;

public:
    using SizeType = std::uint32_t;

    struct InsertResult final {
        const T* value = nullptr;
        bool inserted = false;
    };

    template<bool IsConst>
    class IteratorBase final {
    private:
        using MapIterator = typename std::conditional<IsConst,
            typename Map::ConstIterator, typename Map::Iterator>::type;

    public:
        IteratorBase() noexcept = default;

        AERO_NODISCARD const T& operator*() const noexcept {
            return iterator_->Key();
        }

        AERO_NODISCARD const T* operator->() const noexcept {
            return &iterator_->Key();
        }

        IteratorBase& operator++() noexcept {
            ++iterator_;
            return *this;
        }

        AERO_NODISCARD bool operator==(const IteratorBase& other) const noexcept {
            return iterator_ == other.iterator_;
        }

        AERO_NODISCARD bool operator!=(const IteratorBase& other) const noexcept {
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

    AERO_NODISCARD SizeType Size() const noexcept { return map_.Size(); }
    AERO_NODISCARD SizeType Capacity() const noexcept { return map_.Capacity(); }
    AERO_NODISCARD bool Empty() const noexcept { return map_.Empty(); }
    AERO_NODISCARD HashCode Seed() const noexcept { return map_.Seed(); }
    AERO_NODISCARD IAllocator& Allocator() const noexcept { return map_.Allocator(); }

    AERO_NODISCARD Iterator begin() noexcept { return Iterator(map_.begin()); }
    AERO_NODISCARD Iterator end() noexcept { return Iterator(map_.end()); }
    AERO_NODISCARD ConstIterator begin() const noexcept {
        return ConstIterator(map_.begin());
    }
    AERO_NODISCARD ConstIterator end() const noexcept {
        return ConstIterator(map_.end());
    }

    void Clear() noexcept { map_.Clear(); }

    AERO_NODISCARD Result<void> TryReserve(SizeType expectedElements) noexcept {
        return map_.TryReserve(expectedElements);
    }

    AERO_NODISCARD bool Contains(const T& value) const noexcept {
        return map_.Contains(value);
    }

    AERO_NODISCARD bool Erase(const T& value) noexcept {
        return map_.Erase(value);
    }

    AERO_NODISCARD Result<InsertResult> TryInsert(const T& value) noexcept {
        Result<typename Map::InsertResult> result = map_.TryInsert(
            value, Detail::HashSetMarker{});
        if (!result) {
            return result.GetStatus();
        }
        return InsertResult{&result.Value().entry->Key(), result.Value().inserted};
    }

    AERO_NODISCARD Result<InsertResult> TryInsert(T&& value) noexcept {
        Result<typename Map::InsertResult> result = map_.TryInsert(
            std::move(value), Detail::HashSetMarker{});
        if (!result) {
            return result.GetStatus();
        }
        return InsertResult{&result.Value().entry->Key(), result.Value().inserted};
    }

private:
    Map map_;
};

} // namespace Aero::Base

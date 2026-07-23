#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {
namespace Detail {

template<class T, std::uint32_t Count>
class InlineBuffer {
protected:
    using Slot = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    T* InlineData() noexcept {
        return reinterpret_cast<T*>(slots_);
    }

    const T* InlineData() const noexcept {
        return reinterpret_cast<const T*>(slots_);
    }

private:
    Slot slots_[Count];
};

template<class T>
class InlineBuffer<T, 0U> {
protected:
    T* InlineData() noexcept { return nullptr; }
    const T* InlineData() const noexcept { return nullptr; }
};

template<class T, std::uint32_t InlineCount>
class BasicVector final : private InlineBuffer<T, InlineCount> {
public:
    using ValueType = T;
    using SizeType = std::uint32_t;

    explicit BasicVector(IAllocator* allocator = nullptr) noexcept
        : allocator_(allocator != nullptr ? allocator : &GetDefaultAllocator()) {
        ResetEmptyStorage();
    }

    BasicVector(const BasicVector& other)
        : allocator_(&other.Allocator()) {
        ResetEmptyStorage();
        const Result<void> result = TryAppend(other.AsSpan());
        if (!result) {
            ReportOutOfMemory(BytesForCount(other.size_), alignof(T), MemoryTag::Container);
        }
    }

    BasicVector(BasicVector&& other) noexcept
        : allocator_(other.allocator_) {
        ResetEmptyStorage();
        MoveConstructFrom(other);
    }

    ~BasicVector() {
        DestroyRange(data_, size_);
        ReleaseHeap();
    }

    BasicVector& operator=(const BasicVector& other) {
        if (this != &other) {
            const Result<void> result = TryAssign(other.AsSpan());
            if (!result) {
                ReportOutOfMemory(BytesForCount(other.size_), alignof(T), MemoryTag::Container);
            }
        }
        return *this;
    }

    BasicVector& operator=(BasicVector&& other) noexcept {
        if (this != &other) {
            if (allocator_ == other.allocator_) {
                DestroyRange(data_, size_);
                ReleaseHeap();
                ResetEmptyStorage();
                MoveConstructFrom(other);
            } else {
                BasicVector temporary(allocator_);
                const Result<void> reserveResult = temporary.TryReserve(other.size_);
                if (!reserveResult) {
                    ReportOutOfMemory(BytesForCount(other.size_), alignof(T), MemoryTag::Container);
                }
                for (SizeType index = 0U; index < other.size_; ++index) {
                    const Result<T*> appendResult = temporary.TryEmplaceBack(
                        std::move_if_noexcept(other.data_[index]));
                    if (!appendResult) {
                        ReportOutOfMemory(BytesForCount(other.size_), alignof(T), MemoryTag::Container);
                    }
                }
                other.Clear();
                other.ReleaseHeap();
                other.ResetEmptyStorage();
                AdoptStorageFrom(temporary);
            }
        }
        return *this;
    }

    T* Data() noexcept { return data_; }
    const T* Data() const noexcept { return data_; }
    SizeType Size() const noexcept { return size_; }
    SizeType Capacity() const noexcept { return capacity_; }
    bool Empty() const noexcept { return size_ == 0U; }
    IAllocator& Allocator() const noexcept { return *allocator_; }

    Span<T> AsSpan() noexcept { return {data_, size_}; }
    Span<const T> AsSpan() const noexcept { return {data_, size_}; }

    T& operator[](SizeType index) noexcept {
        AERO_ASSERT(index < size_);
        return data_[index];
    }

    const T& operator[](SizeType index) const noexcept {
        AERO_ASSERT(index < size_);
        return data_[index];
    }

    T& Front() noexcept {
        AERO_ASSERT(size_ > 0U);
        return data_[0];
    }

    const T& Front() const noexcept {
        AERO_ASSERT(size_ > 0U);
        return data_[0];
    }

    T& Back() noexcept {
        AERO_ASSERT(size_ > 0U);
        return data_[size_ - 1U];
    }

    const T& Back() const noexcept {
        AERO_ASSERT(size_ > 0U);
        return data_[size_ - 1U];
    }

    T* begin() noexcept { return data_; }
    const T* begin() const noexcept { return data_; }
    T* end() noexcept { return size_ == 0U ? data_ : data_ + size_; }
    const T* end() const noexcept {
        return size_ == 0U ? data_ : data_ + size_;
    }

    void Clear() noexcept {
        DestroyRange(data_, size_);
        size_ = 0U;
    }

    void PopBack() noexcept {
        AERO_ASSERT(size_ > 0U);
        --size_;
        data_[size_].~T();
    }

    Result<void> TryReserve(SizeType requestedCapacity) noexcept {
        if (requestedCapacity <= capacity_) {
            return {};
        }

        const std::size_t bytes = BytesForCount(requestedCapacity);
        if (bytes == 0U) {
            return Status::Failure(ErrorCode::OutOfRange,
                "Vector capacity exceeds addressable storage");
        }

        void* allocation = allocator_->Allocate(
            {bytes, alignof(T), MemoryTag::Container});
        if (allocation == nullptr) {
            return Status::Failure(ErrorCode::OutOfMemory,
                "Vector allocation failed");
        }

        T* replacement = static_cast<T*>(allocation);
        RelocateConstruct(replacement, data_, size_);
        DestroyRange(data_, size_);
        ReleaseHeap();
        data_ = replacement;
        capacity_ = requestedCapacity;
        return {};
    }

    Result<void> TryResize(SizeType requestedSize) noexcept {
        if (requestedSize < size_) {
            DestroyRange(data_ + requestedSize, size_ - requestedSize);
            size_ = requestedSize;
            return {};
        }

        if (requestedSize == size_) {
            return {};
        }

        const Result<void> reserveResult = EnsureCapacity(requestedSize);
        if (!reserveResult) {
            return reserveResult.GetStatus();
        }

        while (size_ < requestedSize) {
            new (data_ + size_) T();
            ++size_;
        }
        return {};
    }

    Result<void> TryResize(
        SizeType requestedSize, const T& value) noexcept {
        if (requestedSize < size_) {
            DestroyRange(data_ + requestedSize, size_ - requestedSize);
            size_ = requestedSize;
            return {};
        }

        if (requestedSize == size_) {
            return {};
        }

        const Result<void> reserveResult = EnsureCapacity(requestedSize);
        if (!reserveResult) {
            return reserveResult.GetStatus();
        }

        while (size_ < requestedSize) {
            new (data_ + size_) T(value);
            ++size_;
        }
        return {};
    }

    template<class... Args>
    Result<T*> TryEmplaceBack(Args&&... args) noexcept {
        if (size_ == UINT32_MAX) {
            return Status::Failure(ErrorCode::OutOfRange,
                "Vector size limit reached");
        }

        const Result<void> reserveResult = EnsureCapacity(size_ + 1U);
        if (!reserveResult) {
            return reserveResult.GetStatus();
        }

        T* value = new (data_ + size_) T(std::forward<Args>(args)...);
        ++size_;
        return value;
    }

    Result<void> TryPushBack(const T& value) noexcept {
        const Result<T*> result = TryEmplaceBack(value);
        return result ? Result<void>() : Result<void>(result.GetStatus());
    }

    Result<void> TryPushBack(T&& value) noexcept {
        const Result<T*> result = TryEmplaceBack(std::move(value));
        return result ? Result<void>() : Result<void>(result.GetStatus());
    }

    Result<void> TryAssign(Span<const T> values) noexcept {
        if (IsAliased(values.Data(), values.Size())) {
            BasicVector temporary(allocator_);
            const Result<void> temporaryResult = temporary.TryAppend(values);
            if (!temporaryResult) {
                return temporaryResult.GetStatus();
            }
            AdoptStorageFrom(temporary);
            return {};
        }

        const Result<void> reserveResult = TryReserve(values.Size());
        if (!reserveResult) {
            return reserveResult.GetStatus();
        }

        DestroyRange(data_, size_);
        size_ = 0U;
        for (const T& value : values) {
            new (data_ + size_) T(value);
            ++size_;
        }
        return {};
    }

    Result<void> TryAppend(Span<const T> values) noexcept {
        if (values.Empty()) {
            return {};
        }

        if (values.Size() > UINT32_MAX - size_) {
            return Status::Failure(ErrorCode::OutOfRange,
                "Vector append exceeds size limit");
        }

        const bool aliased = IsAliased(values.Data(), values.Size());
        const SizeType sourceOffset = aliased
            ? static_cast<SizeType>(values.Data() - data_)
            : 0U;
        const SizeType sourceCount = values.Size();
        const Result<void> reserveResult = EnsureCapacity(size_ + sourceCount);
        if (!reserveResult) {
            return reserveResult.GetStatus();
        }

        const T* source = aliased ? data_ + sourceOffset : values.Data();
        for (SizeType index = 0U; index < sourceCount; ++index) {
            new (data_ + size_) T(source[index]);
            ++size_;
        }
        return {};
    }

private:
    IAllocator* allocator_ = nullptr;
    T* data_ = nullptr;
    SizeType size_ = 0U;
    SizeType capacity_ = 0U;

    bool IsInline() const noexcept {
        if constexpr (InlineCount == 0U) {
            return false;
        } else {
            return data_ == this->InlineData();
        }
    }

    bool UsesHeap() const noexcept {
        return data_ != nullptr && !IsInline();
    }

    void ResetEmptyStorage() noexcept {
        if constexpr (InlineCount == 0U) {
            data_ = nullptr;
            capacity_ = 0U;
        } else {
            data_ = this->InlineData();
            capacity_ = InlineCount;
        }
        size_ = 0U;
    }

    void ReleaseHeap() noexcept {
        if (!UsesHeap()) {
            return;
        }
        allocator_->Deallocate(data_, BytesForCount(capacity_),
            alignof(T), MemoryTag::Container);
    }

    static std::size_t BytesForCount(SizeType count) noexcept {
        if (count == 0U) {
            return 0U;
        }
        if (static_cast<std::uint64_t>(count) >
            static_cast<std::uint64_t>(SIZE_MAX / sizeof(T))) {
            return 0U;
        }
        return static_cast<std::size_t>(count) * sizeof(T);
    }

    Result<void> EnsureCapacity(SizeType required) noexcept {
        if (required <= capacity_) {
            return {};
        }

        SizeType grown = capacity_ == 0U ? 4U : capacity_;
        while (grown < required) {
            const SizeType increment = grown / 2U + 1U;
            if (increment > UINT32_MAX - grown) {
                grown = required;
                break;
            }
            grown += increment;
        }
        if (grown < required) {
            grown = required;
        }
        return TryReserve(grown);
    }

    bool IsAliased(
        const T* source, SizeType count) const noexcept {
        if (source == nullptr || count == 0U || data_ == nullptr || size_ == 0U) {
            return false;
        }
        const std::uintptr_t beginAddress =
            reinterpret_cast<std::uintptr_t>(data_);
        const std::uintptr_t endAddress = beginAddress +
            static_cast<std::uintptr_t>(size_) * sizeof(T);
        const std::uintptr_t sourceAddress =
            reinterpret_cast<std::uintptr_t>(source);
        if (sourceAddress < beginAddress || sourceAddress > endAddress) {
            return false;
        }
        const std::uintptr_t remainingBytes = endAddress - sourceAddress;
        return static_cast<std::uint64_t>(count) * sizeof(T) <=
            remainingBytes;
    }

    static void DestroyRange(T* values, SizeType count) noexcept {
        if constexpr (!std::is_trivially_destructible<T>::value) {
            for (SizeType index = count; index > 0U; --index) {
                values[index - 1U].~T();
            }
        }
    }

    static void RelocateConstruct(T* destination, T* source, SizeType count) noexcept {
        if constexpr (std::is_trivially_copyable<T>::value) {
            if (count > 0U) {
                std::memcpy(destination, source,
                    static_cast<std::size_t>(count) * sizeof(T));
            }
        } else {
            for (SizeType index = 0U; index < count; ++index) {
                new (destination + index) T(std::move_if_noexcept(source[index]));
            }
        }
    }

    void MoveConstructFrom(BasicVector& other) noexcept {
        if (other.UsesHeap()) {
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.ResetEmptyStorage();
            return;
        }

        for (SizeType index = 0U; index < other.size_; ++index) {
            new (data_ + index) T(std::move_if_noexcept(other.data_[index]));
        }
        size_ = other.size_;
        DestroyRange(other.data_, other.size_);
        other.size_ = 0U;
    }

    void AdoptStorageFrom(BasicVector& other) noexcept {
        AERO_ASSERT(allocator_ == other.allocator_);
        DestroyRange(data_, size_);
        ReleaseHeap();
        ResetEmptyStorage();
        MoveConstructFrom(other);
    }
};

} // namespace Detail

template<class T>
using Vector = Detail::BasicVector<T, 0U>;

template<class T, std::uint32_t InlineCount>
using SmallVector = Detail::BasicVector<T, InlineCount>;

} // namespace Aero::Base

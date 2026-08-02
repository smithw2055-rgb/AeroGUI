#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <limits>
#include <utility>

namespace Aero::Base {
namespace {

constexpr Status OutOfMemoryStatus() noexcept {
    return Status::Failure(ErrorCode::OutOfMemory,
        "Unable to allocate Aero string storage");
}

constexpr Status StringTooLargeStatus() noexcept {
    return Status::Failure(ErrorCode::OutOfRange,
        "Aero string exceeds the 32-bit byte length limit");
}

constexpr Status InvalidUtf8Status() noexcept {
    return Status::Failure(ErrorCode::InvalidUtf8,
        "Input is not valid UTF-8");
}

} // namespace

String::String(IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &GetDefaultAllocator()),
      data_(inlineStorage_) {
    inlineStorage_[0] = '\0';
}

String::String(const String& other)
    : String(&other.Allocator()) {
    const Result<void> result = AssignUnchecked(other.View());
    if (!result) {
        ReportOutOfMemory(
            static_cast<std::size_t>(other.SizeBytes()) + 1U,
            alignof(char),
            MemoryTag::String);
    }
}

String::String(String&& other) noexcept {
    MoveFrom(std::move(other));
}

String::~String() {
    ReleaseHeap();
}

String& String::operator=(const String& other) {
    if (this != &other) {
        const Result<void> result = AssignUnchecked(other.View());
        if (!result) {
            ReportOutOfMemory(
                static_cast<std::size_t>(other.SizeBytes()) + 1U,
                alignof(char),
                MemoryTag::String);
        }
    }
    return *this;
}

String& String::operator=(String&& other) noexcept {
    if (this != &other) {
        ReleaseHeap();
        MoveFrom(std::move(other));
    }
    return *this;
}

void String::Clear() noexcept {
    size_ = 0U;
    data_[0] = '\0';
}

Result<void> String::Reserve(std::uint32_t capacityBytes) noexcept {
    return EnsureCapacity(capacityBytes);
}

Result<void> String::Assign(StringView utf8) noexcept {
    if (utf8.SizeBytes() > 0U && utf8.Data() == nullptr) {
        return Status::Failure(ErrorCode::InvalidArgument,
            "StringView has a null data pointer and a non-zero size");
    }
    if (!ValidateUtf8(utf8).valid) {
        return InvalidUtf8Status();
    }
    return AssignUnchecked(utf8);
}

Result<void> String::AssignUnchecked(StringView bytes) noexcept {
    if (bytes.SizeBytes() > 0U && bytes.Data() == nullptr) {
        return Status::Failure(ErrorCode::InvalidArgument,
            "StringView has a null data pointer and a non-zero size");
    }

    const std::uintptr_t sourceAddress =
        reinterpret_cast<std::uintptr_t>(bytes.Data());
    const std::uintptr_t beginAddress =
        reinterpret_cast<std::uintptr_t>(data_);
    const std::uintptr_t endAddress = beginAddress + size_;
    if (sourceAddress >= beginAddress && sourceAddress <= endAddress) {
        const std::uint32_t offset = static_cast<std::uint32_t>(
            sourceAddress - beginAddress);
        if (bytes.SizeBytes() > size_ - std::min(offset, size_)) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Overlapping string assignment is outside the current value");
        }

        if (offset != 0U && bytes.SizeBytes() > 0U) {
            std::memmove(data_, data_ + offset, bytes.SizeBytes());
        }
        size_ = bytes.SizeBytes();
        data_[size_] = '\0';
        return {};
    }

    Result<void> capacityResult = EnsureCapacity(bytes.SizeBytes());
    if (!capacityResult) {
        return capacityResult;
    }

    if (bytes.SizeBytes() > 0U) {
        std::memcpy(data_, bytes.Data(), bytes.SizeBytes());
    }
    size_ = bytes.SizeBytes();
    data_[size_] = '\0';
    return {};
}

Result<void> String::Append(StringView utf8) noexcept {
    if (utf8.SizeBytes() > 0U && utf8.Data() == nullptr) {
        return Status::Failure(ErrorCode::InvalidArgument,
            "StringView has a null data pointer and a non-zero size");
    }
    if (!ValidateUtf8(utf8).valid) {
        return InvalidUtf8Status();
    }
    return AppendUnchecked(utf8);
}

Result<void> String::AppendUnchecked(StringView bytes) noexcept {
    if (bytes.SizeBytes() > 0U && bytes.Data() == nullptr) {
        return Status::Failure(ErrorCode::InvalidArgument,
            "StringView has a null data pointer and a non-zero size");
    }

    if (bytes.SizeBytes() >
        std::numeric_limits<std::uint32_t>::max() - size_) {
        return StringTooLargeStatus();
    }

    const std::uint32_t oldSize = size_;
    const std::uint32_t newSize = oldSize + bytes.SizeBytes();

    const std::uintptr_t sourceAddress =
        reinterpret_cast<std::uintptr_t>(bytes.Data());
    const std::uintptr_t beginAddress =
        reinterpret_cast<std::uintptr_t>(data_);
    const std::uintptr_t endAddress = beginAddress + oldSize;
    const bool aliasesCurrent =
        sourceAddress >= beginAddress && sourceAddress <= endAddress;
    std::uint32_t aliasOffset = 0U;
    if (aliasesCurrent) {
        aliasOffset = static_cast<std::uint32_t>(sourceAddress - beginAddress);
        if (bytes.SizeBytes() > oldSize - std::min(aliasOffset, oldSize)) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Overlapping string append is outside the current value");
        }
    }

    Result<void> capacityResult = EnsureCapacity(newSize);
    if (!capacityResult) {
        return capacityResult;
    }

    const char* source = aliasesCurrent ? data_ + aliasOffset : bytes.Data();
    if (bytes.SizeBytes() > 0U) {
        std::memmove(data_ + oldSize, source, bytes.SizeBytes());
    }
    size_ = newSize;
    data_[size_] = '\0';
    return {};
}

void String::ReleaseHeap() noexcept {
    if (data_ != nullptr && !IsInline()) {
        allocator_->Deallocate(
            data_,
            static_cast<std::size_t>(capacity_) + 1U,
            alignof(char),
            MemoryTag::String);
    }
}

void String::ResetInline() noexcept {
    data_ = inlineStorage_;
    size_ = 0U;
    capacity_ = InlineCapacity;
    inlineStorage_[0] = '\0';
}

void String::MoveFrom(String&& other) noexcept {
    allocator_ = other.allocator_ != nullptr
        ? other.allocator_
        : &GetDefaultAllocator();

    if (other.data_ == other.inlineStorage_) {
        data_ = inlineStorage_;
        size_ = other.size_;
        capacity_ = InlineCapacity;
        if (size_ > 0U) {
            std::memcpy(inlineStorage_, other.inlineStorage_, size_);
        }
        inlineStorage_[size_] = '\0';
        other.ResetInline();
        return;
    }

    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.ResetInline();
}

Result<void> String::EnsureCapacity(std::uint32_t required) noexcept {
    if (required <= capacity_) {
        return {};
    }

    std::uint64_t grown = static_cast<std::uint64_t>(capacity_) +
        static_cast<std::uint64_t>(capacity_ / 2U) + 8U;
    if (grown < required) {
        grown = required;
    }

    if (grown > std::numeric_limits<std::uint32_t>::max()) {
        return StringTooLargeStatus();
    }

    const std::uint32_t newCapacity = static_cast<std::uint32_t>(grown);
    void* allocation = allocator_->Allocate({
        static_cast<std::size_t>(newCapacity) + 1U,
        alignof(char),
        MemoryTag::String});
    if (allocation == nullptr) {
        return OutOfMemoryStatus();
    }

    auto* newData = static_cast<char*>(allocation);
    if (size_ > 0U) {
        std::memcpy(newData, data_, size_);
    }
    newData[size_] = '\0';

    if (!IsInline()) {
        allocator_->Deallocate(
            data_,
            static_cast<std::size_t>(capacity_) + 1U,
            alignof(char),
            MemoryTag::String);
    }

    data_ = newData;
    capacity_ = newCapacity;
    return {};
}

} // namespace Aero::Base

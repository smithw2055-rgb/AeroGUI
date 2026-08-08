#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Base {

class AERO_BASE_API String {
public:
    static constexpr std::uint32_t InlineCapacity = 23U;

    explicit String(IAllocator* allocator = nullptr) noexcept;
    String(const String& other);
    String(String&& other) noexcept;
    ~String();

    String& operator=(const String& other);
    String& operator=(String&& other) noexcept;

    StringView View() const noexcept {
        return {data_, size_};
    }

    const char* CStr() const noexcept { return data_; }
    std::uint32_t SizeBytes() const noexcept { return size_; }
    std::uint32_t CapacityBytes() const noexcept { return capacity_; }
    bool Empty() const noexcept { return size_ == 0U; }
    IAllocator& Allocator() const noexcept { return *allocator_; }

    void Clear() noexcept;

    Result<void> Reserve(std::uint32_t capacityBytes) noexcept;
    Result<void> Assign(StringView utf8) noexcept;
    Result<void> AssignUnchecked(StringView bytes) noexcept;
    Result<void> Append(StringView utf8) noexcept;
    Result<void> AppendUnchecked(StringView bytes) noexcept;

private:
    IAllocator* allocator_ = nullptr;
    char* data_ = nullptr;
    std::uint32_t size_ = 0;
    std::uint32_t capacity_ = InlineCapacity;
    alignas(void*) char inlineStorage_[InlineCapacity + 1U]{};

    bool IsInline() const noexcept {
        return data_ == inlineStorage_;
    }

    void ReleaseHeap() noexcept;
    void ResetInline() noexcept;
    void MoveFrom(String&& other) noexcept;

    Result<void> EnsureCapacity(std::uint32_t required) noexcept;
};

inline bool operator==(const String& left, StringView right) noexcept {
    return left.View() == right;
}

inline bool operator!=(const String& left, StringView right) noexcept {
    return !(left == right);
}

} // namespace Aero::Base

#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Base {

class String final {
public:
    static constexpr std::uint32_t InlineCapacity = 23U;

    explicit String(IAllocator* allocator = nullptr) noexcept;
    String(const String& other);
    String(String&& other) noexcept;
    ~String();

    String& operator=(const String& other);
    String& operator=(String&& other) noexcept;

    AERO_NODISCARD StringView View() const noexcept {
        return {data_, size_};
    }

    AERO_NODISCARD const char* CStr() const noexcept { return data_; }
    AERO_NODISCARD std::uint32_t SizeBytes() const noexcept { return size_; }
    AERO_NODISCARD std::uint32_t CapacityBytes() const noexcept { return capacity_; }
    AERO_NODISCARD bool Empty() const noexcept { return size_ == 0U; }
    AERO_NODISCARD IAllocator& Allocator() const noexcept { return *allocator_; }

    void Clear() noexcept;

    AERO_NODISCARD Result<void> TryReserve(std::uint32_t capacityBytes) noexcept;
    AERO_NODISCARD Result<void> TryAssign(StringView utf8) noexcept;
    AERO_NODISCARD Result<void> TryAssignUnchecked(StringView bytes) noexcept;
    AERO_NODISCARD Result<void> TryAppend(StringView utf8) noexcept;
    AERO_NODISCARD Result<void> TryAppendUnchecked(StringView bytes) noexcept;

private:
    IAllocator* allocator_ = nullptr;
    char* data_ = nullptr;
    std::uint32_t size_ = 0;
    std::uint32_t capacity_ = InlineCapacity;
    alignas(void*) char inlineStorage_[InlineCapacity + 1U]{};

    AERO_NODISCARD bool IsInline() const noexcept {
        return data_ == inlineStorage_;
    }

    void ReleaseHeap() noexcept;
    void ResetInline() noexcept;
    void MoveFrom(String&& other) noexcept;

    AERO_NODISCARD Result<void> EnsureCapacity(std::uint32_t required) noexcept;
};

AERO_NODISCARD inline bool operator==(const String& left, StringView right) noexcept {
    return left.View() == right;
}

AERO_NODISCARD inline bool operator!=(const String& left, StringView right) noexcept {
    return !(left == right);
}

} // namespace Aero::Base

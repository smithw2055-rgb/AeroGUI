#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>

#include <cstring>

namespace Aero::Base {

class StringView final {
public:
    constexpr StringView() noexcept = default;

    constexpr StringView(const char* data, std::uint32_t size) noexcept
        : data_(data), size_(size) {
        AERO_ASSERT(data != nullptr || size == 0U);
    }

    template<std::size_t N>
    constexpr StringView(const char (&literal)[N]) noexcept
        : data_(literal), size_(static_cast<std::uint32_t>(N - 1U)) {
        static_assert(N > 0U, "String literal must include a terminator");
        static_assert(N - 1U <= UINT32_MAX, "String literal is too large");
    }

    AERO_NODISCARD constexpr const char* Data() const noexcept { return data_; }
    AERO_NODISCARD constexpr std::uint32_t SizeBytes() const noexcept { return size_; }
    AERO_NODISCARD constexpr bool Empty() const noexcept { return size_ == 0U; }

    AERO_NODISCARD constexpr char operator[](std::uint32_t index) const noexcept {
        AERO_ASSERT(index < size_);
        return data_[index];
    }

    AERO_NODISCARD constexpr const char* begin() const noexcept { return data_; }
    AERO_NODISCARD constexpr const char* end() const noexcept {
        return size_ == 0U ? data_ : data_ + size_;
    }

    AERO_NODISCARD constexpr StringView Substr(
        std::uint32_t offset, std::uint32_t count) const noexcept {
        AERO_ASSERT(offset <= size_);
        AERO_ASSERT(count <= size_ - offset);
        return {offset == 0U ? data_ : data_ + offset, count};
    }

    AERO_NODISCARD int Compare(StringView other) const noexcept {
        const std::uint32_t common = size_ < other.size_ ? size_ : other.size_;
        if (common > 0U) {
            const int result = std::memcmp(data_, other.data_, common);
            if (result != 0) {
                return result;
            }
        }
        return size_ < other.size_ ? -1 : (size_ > other.size_ ? 1 : 0);
    }

private:
    const char* data_ = nullptr;
    std::uint32_t size_ = 0;
};

AERO_NODISCARD inline bool operator==(StringView left, StringView right) noexcept {
    return left.SizeBytes() == right.SizeBytes() && left.Compare(right) == 0;
}

AERO_NODISCARD inline bool operator!=(StringView left, StringView right) noexcept {
    return !(left == right);
}

} // namespace Aero::Base

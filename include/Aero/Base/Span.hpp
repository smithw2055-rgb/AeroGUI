#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>

#include <type_traits>

namespace Aero::Base {

template<class T>
class Span final {
public:
    using ElementType = T;

    constexpr Span() noexcept = default;

    constexpr Span(T* data, std::uint32_t size) noexcept
        : data_(data), size_(size) {
        AERO_ASSERT(data != nullptr || size == 0U);
    }

    template<std::size_t N>
    constexpr Span(T (&items)[N]) noexcept
        : data_(items), size_(static_cast<std::uint32_t>(N)) {
        static_assert(N <= UINT32_MAX, "Span array is too large");
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U (*)[], T (*)[]>::value>>
    constexpr Span(const Span<U>& other) noexcept
        : data_(other.Data()), size_(other.Size()) {}

    constexpr T* Data() const noexcept { return data_; }
    constexpr std::uint32_t Size() const noexcept { return size_; }
    constexpr bool Empty() const noexcept { return size_ == 0U; }

    constexpr T& operator[](std::uint32_t index) const noexcept {
        AERO_ASSERT(index < size_);
        return data_[index];
    }

    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end() const noexcept {
        return size_ == 0U ? data_ : data_ + size_;
    }

    constexpr Span Subspan(
        std::uint32_t offset, std::uint32_t count) const noexcept {
        AERO_ASSERT(offset <= size_);
        AERO_ASSERT(count <= size_ - offset);
        return {offset == 0U ? data_ : data_ + offset, count};
    }

private:
    T* data_ = nullptr;
    std::uint32_t size_ = 0;
};

} // namespace Aero::Base

#pragma once

#include <cstdint>

namespace Aero {

struct VisualHandle {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

constexpr bool operator==(VisualHandle left, VisualHandle right) noexcept {
    return left.index == right.index && left.generation == right.generation;
}

constexpr bool operator!=(VisualHandle left, VisualHandle right) noexcept {
    return !(left == right);
}

} // namespace Aero

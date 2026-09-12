#pragma once

#include <cstdint>

namespace Aero::Diagnostics {

struct SourcePosition {
    // Line and column are one-based. A zero pair represents an unknown position.
    std::uint32_t line = 0U;
    std::uint32_t column = 0U;
    std::uint64_t byteOffset = 0U;

    constexpr bool IsKnown() const noexcept {
        return line != 0U || column != 0U;
    }
};

struct SourceSpan {
    // End is exclusive when the source provider can identify it precisely.
    SourcePosition begin;
    SourcePosition end;
};

} // namespace Aero::Diagnostics

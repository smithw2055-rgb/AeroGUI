#pragma once

#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Base {

constexpr bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' ||
        value == '\r' || value == '\f' || value == '\v';
}

constexpr char ToAsciiLower(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

inline StringView TrimAscii(StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end && IsAsciiWhitespace(value[begin])) ++begin;
    while (end > begin && IsAsciiWhitespace(value[end - 1U])) --end;
    return value.Substr(begin, end - begin);
}

inline bool EqualsAsciiInsensitive(
    StringView left,
    StringView right) noexcept {
    if (left.SizeBytes() != right.SizeBytes()) return false;
    for (std::uint32_t index = 0U;
         index < left.SizeBytes(); ++index) {
        if (ToAsciiLower(left[index]) != ToAsciiLower(right[index])) {
            return false;
        }
    }
    return true;
}

} // namespace Aero::Base

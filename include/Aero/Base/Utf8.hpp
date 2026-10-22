#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Base {

struct Utf8Validation {
    bool valid = true;
    std::uint32_t errorOffset = 0;
};

struct DecodedCodePoint {
    std::uint32_t value = 0U;
    std::uint32_t length = 0U;
};

AERO_BASE_API Utf8Validation ValidateUtf8(StringView text) noexcept;
AERO_BASE_API DecodedCodePoint DecodeCodePoint(const char* data, std::uint32_t size) noexcept;
AERO_BASE_API std::uint32_t EncodeCodePoint(std::uint32_t codePoint, char* outBuffer) noexcept;

} // namespace Aero::Base

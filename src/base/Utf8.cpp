#include <Aero/Base/Utf8.hpp>

namespace Aero::Base {

Utf8Validation ValidateUtf8(StringView text) noexcept {
    const std::uint32_t size = text.SizeBytes();
    if (size > 0U && text.Data() == nullptr) {
        return {false, 0U};
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(text.Data());
    std::uint32_t index = 0;
    while (index < size) {
        const unsigned char lead = bytes[index];
        if (lead <= 0x7FU) {
            ++index;
            continue;
        }

        std::uint32_t sequenceLength = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;

        if ((lead & 0xE0U) == 0xC0U) {
            sequenceLength = 2;
            codePoint = static_cast<std::uint32_t>(lead & 0x1FU);
            minimum = 0x80U;
        } else if ((lead & 0xF0U) == 0xE0U) {
            sequenceLength = 3;
            codePoint = static_cast<std::uint32_t>(lead & 0x0FU);
            minimum = 0x800U;
        } else if ((lead & 0xF8U) == 0xF0U) {
            sequenceLength = 4;
            codePoint = static_cast<std::uint32_t>(lead & 0x07U);
            minimum = 0x10000U;
        } else {
            return {false, index};
        }

        if (sequenceLength > size - index) {
            return {false, index};
        }

        for (std::uint32_t offset = 1; offset < sequenceLength; ++offset) {
            const unsigned char continuation = bytes[index + offset];
            if ((continuation & 0xC0U) != 0x80U) {
                return {false, index + offset};
            }
            codePoint = (codePoint << 6U) |
                static_cast<std::uint32_t>(continuation & 0x3FU);
        }

        if (codePoint < minimum ||
            codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return {false, index};
        }

        index += sequenceLength;
    }

    return {true, 0U};
}

} // namespace Aero::Base

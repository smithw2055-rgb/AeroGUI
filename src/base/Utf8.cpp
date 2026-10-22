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

DecodedCodePoint DecodeCodePoint(const char* data, std::uint32_t size) noexcept {
    if (data == nullptr || size == 0U) {
        return {0U, 0U};
    }
    const auto lead = static_cast<std::uint8_t>(data[0]);
    if (lead <= 0x7FU) {
        return {lead, 1U};
    }
    std::uint32_t length = 0U;
    std::uint32_t value = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
        length = 2U;
        value = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0U) {
        length = 3U;
        value = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0U) {
        length = 4U;
        value = lead & 0x07U;
    } else {
        return {0xFFFDU, 1U};
    }
    if (length > size) {
        return {0xFFFDU, 1U};
    }
    for (std::uint32_t index = 1U; index < length; ++index) {
        const auto byte = static_cast<std::uint8_t>(data[index]);
        if ((byte & 0xC0U) != 0x80U) {
            return {0xFFFDU, 1U};
        }
        value = (value << 6U) | (byte & 0x3FU);
    }
    return {value, length};
}

std::uint32_t EncodeCodePoint(std::uint32_t codePoint, char* outBuffer) noexcept {
    if (outBuffer == nullptr) {
        return 0U;
    }
    if (codePoint <= 0x7FU) {
        outBuffer[0] = static_cast<char>(codePoint);
        return 1U;
    }
    if (codePoint <= 0x7FFU) {
        outBuffer[0] = static_cast<char>(0xC0U | ((codePoint >> 6U) & 0x1FU));
        outBuffer[1] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        return 2U;
    }
    if (codePoint <= 0xFFFFU) {
        outBuffer[0] = static_cast<char>(0xE0U | ((codePoint >> 12U) & 0x0FU));
        outBuffer[1] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        outBuffer[2] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        return 3U;
    }
    if (codePoint <= 0x10FFFFU) {
        outBuffer[0] = static_cast<char>(0xF0U | ((codePoint >> 18U) & 0x07U));
        outBuffer[1] = static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU));
        outBuffer[2] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        outBuffer[3] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        return 4U;
    }
    return 0U;
}

} // namespace Aero::Base


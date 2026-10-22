#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>
#include <cstring>

namespace Aero::Markup {

inline Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    output.PushBack(value);
    return {};
}

inline Base::Result<void> AppendU16(
    Base::Vector<std::uint8_t>& output,
    std::uint16_t value) noexcept {
    output.PushBack(static_cast<std::uint8_t>(value));
    output.PushBack(static_cast<std::uint8_t>(value >> 8U));
    return {};
}

inline Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

inline Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

inline Base::Result<void> AppendVarU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7FU);
        value >>= 7U;
        if (value != 0U) {
            byte = static_cast<std::uint8_t>(byte | 0x80U);
        }
        output.PushBack(byte);
    } while (value != 0U);
    return {};
}

inline Base::Result<void> AppendVarU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    return AppendVarU64(output, value);
}

inline Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> length = AppendU32(output, value.SizeBytes());
    if (!length) return length.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        output.PushBack(static_cast<std::uint8_t>(value[index]));
    }
    return {};
}

class Decoder {
public:
    explicit Decoder(
        Base::Span<const std::uint8_t> bytes,
        const char* truncatedMessage = "Binary payload is truncated") noexcept
        : bytes_(bytes), truncatedMessage_(truncatedMessage) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint16_t> ReadU16() noexcept {
        if (bytes_.Size() - offset_ < 2U) return Truncated();
        const std::uint16_t value = static_cast<std::uint16_t>(bytes_[offset_]) |
            static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
        offset_ += 2U;
        return value;
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadVarU64() noexcept {
        std::uint64_t value = 0U;
        for (std::uint32_t byteIndex = 0U; byteIndex < 10U; ++byteIndex) {
            Base::Result<std::uint8_t> byte = ReadU8();
            if (!byte) return byte.GetStatus();
            const std::uint8_t payload = static_cast<std::uint8_t>(byte.Value() & 0x7FU);
            if (byteIndex == 9U && payload > 1U) {
                return InvalidVarUInt();
            }
            value |= static_cast<std::uint64_t>(payload) << (byteIndex * 7U);
            if ((byte.Value() & 0x80U) == 0U) {
                return value;
            }
        }
        return InvalidVarUInt();
    }

    Base::Result<std::uint32_t> ReadVarU32() noexcept {
        Base::Result<std::uint64_t> value = ReadVarU64();
        if (!value) return value.GetStatus();
        if (value.Value() > UINT32_MAX) {
            return InvalidVarUInt();
        }
        return static_cast<std::uint32_t>(value.Value());
    }

    Base::Result<Base::String> ReadString(
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        return ReadStringInternal(nullptr, totalStringBytes, maxStringBytes);
    }

    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        return ReadStringInternal(&allocator, totalStringBytes, maxStringBytes);
    }

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }
    std::uint32_t Offset() const noexcept { return offset_; }
    Base::Span<const std::uint8_t> RemainingBytes() const noexcept {
        return bytes_.Subspan(offset_, bytes_.Size() - offset_);
    }

private:
    Base::Result<Base::String> ReadStringInternal(
        Base::IAllocator* allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes > maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binary string bounds are invalid");
        }
        Base::String value = allocator != nullptr ? Base::String(allocator) : Base::String();
        Base::Result<void> assigned = value.Assign(
            Base::StringView(
                reinterpret_cast<const char*>(bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    Base::Status Truncated() const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            truncatedMessage_);
    }

    static Base::Status InvalidVarUInt() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binary variable-length integer is invalid");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
    const char* truncatedMessage_ = "Binary payload is truncated";
};

} // namespace Aero::Markup

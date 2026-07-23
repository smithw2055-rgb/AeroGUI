#pragma once

#include <Aero/Base/Hash.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Base {

using MetaTypeId = std::uint64_t;
using MetaMemberId = std::uint64_t;

inline constexpr MetaTypeId InvalidMetaTypeId = 0U;
inline constexpr MetaMemberId InvalidMetaMemberId = 0U;

inline constexpr StringView DefaultMetadataNamespaceUri() noexcept {
    return StringView("urn:aero");
}

namespace Detail {

inline constexpr HashCode StableMetadataIdOffsetBasis =
    UINT64_C(14695981039346656037);
inline constexpr HashCode StableMetadataIdPrime = UINT64_C(1099511628211);
inline constexpr HashCode StableMetadataIdNonZeroFallback =
    UINT64_C(0x9E3779B97F4A7C15);

class StableMetadataIdBuilder final {
public:
    constexpr void AddByte(std::uint8_t value) noexcept {
        value_ ^= static_cast<HashCode>(value);
        value_ *= StableMetadataIdPrime;
    }

    constexpr void AddText(const char* data, std::uint32_t size) noexcept {
        for (std::uint32_t index = 0U; index < size; ++index) {
            AddByte(static_cast<std::uint8_t>(
                static_cast<unsigned char>(data[index])));
        }
    }

    constexpr void AddU32(std::uint32_t value) noexcept {
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    constexpr void AddU64(std::uint64_t value) noexcept {
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    constexpr void AddString(StringView value) noexcept {
        AddU32(value.SizeBytes());
        AddText(value.Data(), value.SizeBytes());
    }

    constexpr std::uint64_t Finish() const noexcept {
        const std::uint64_t result = MixHash64(value_);
        return result != 0U ? result : StableMetadataIdNonZeroFallback;
    }

private:
    HashCode value_ = StableMetadataIdOffsetBasis;
};

} // namespace Detail

constexpr MetaTypeId MakeMetaTypeId(
    StringView xamlNamespace,
    StringView name) noexcept {
    constexpr char domain[] = "AERO.TYPE.V1";
    Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddString(xamlNamespace);
    builder.AddString(name);
    return builder.Finish();
}

constexpr MetaTypeId MakeMetaTypeId(StringView name) noexcept {
    return MakeMetaTypeId(DefaultMetadataNamespaceUri(), name);
}

} // namespace Aero::Base

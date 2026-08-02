#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Aero::Base {

using HashCode = std::uint64_t;

constexpr HashCode MixHash64(HashCode value) noexcept {
    value ^= value >> 30U;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31U;
    return value;
}

inline HashCode HashBytes(
    const void* data, std::uint32_t size, HashCode seed = 0U) noexcept {
    constexpr HashCode OffsetBasis = UINT64_C(14695981039346656037);
    constexpr HashCode Prime = UINT64_C(1099511628211);
    const auto* bytes = static_cast<const unsigned char*>(data);
    HashCode hash = OffsetBasis ^ MixHash64(seed);
    for (std::uint32_t index = 0U; index < size; ++index) {
        hash ^= static_cast<HashCode>(bytes[index]);
        hash *= Prime;
    }
    return MixHash64(hash ^ static_cast<HashCode>(size));
}

template<class T, class Enable = void>
struct DefaultHash;

template<class T>
struct DefaultHash<T, std::enable_if_t<
    std::is_integral<T>::value && !std::is_same<T, bool>::value>> final {
    HashCode operator()(T value, HashCode seed = 0U) const noexcept {
        using Unsigned = typename std::make_unsigned<T>::type;
        return MixHash64(static_cast<HashCode>(static_cast<Unsigned>(value)) ^ seed);
    }
};

template<>
struct DefaultHash<bool, void>  {
    HashCode operator()(
        bool value, HashCode seed = 0U) const noexcept {
        return MixHash64((value ? HashCode{1U} : HashCode{0U}) ^ seed);
    }
};

template<class T>
struct DefaultHash<T, std::enable_if_t<std::is_enum<T>::value>>  {
    HashCode operator()(T value, HashCode seed = 0U) const noexcept {
        using Underlying = typename std::underlying_type<T>::type;
        using Unsigned = typename std::make_unsigned<Underlying>::type;
        return MixHash64(static_cast<HashCode>(
            static_cast<Unsigned>(static_cast<Underlying>(value))) ^ seed);
    }
};

template<class T>
struct DefaultHash<T*, void>  {
    HashCode operator()(const T* value, HashCode seed = 0U) const noexcept {
        return MixHash64(static_cast<HashCode>(
            reinterpret_cast<std::uintptr_t>(value)) ^ seed);
    }
};

template<>
struct DefaultHash<StringView, void>  {
    HashCode operator()(
        StringView value, HashCode seed = 0U) const noexcept {
        return HashBytes(value.Data(), value.SizeBytes(), seed);
    }
};

template<>
struct DefaultHash<String, void>  {
    HashCode operator()(
        const String& value, HashCode seed = 0U) const noexcept {
        return HashBytes(value.View().Data(), value.View().SizeBytes(), seed);
    }
};

template<class T>
struct DefaultEqual  {
    bool operator()(const T& left, const T& right) const
        noexcept(noexcept(left == right)) {
        return left == right;
    }
};

template<>
struct DefaultEqual<String>  {
    bool operator()(
        const String& left, const String& right) const noexcept {
        return left.View() == right.View();
    }
};

} // namespace Aero::Base

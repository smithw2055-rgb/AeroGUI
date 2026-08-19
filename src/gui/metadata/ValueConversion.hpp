#pragma once

// Internal text-to-value conversion helpers used by metadata, markup, media and
// control codecs. Source-only: never included from installed headers.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace Aero::Base {

namespace ValueConversion {

StringView Trim(StringView value) noexcept;
bool EqualsAsciiInsensitive(
    StringView left,
    StringView right) noexcept;
Result<double> ParseDouble(
    StringView text) noexcept;

Result<bool> ConvertBoolean(
    StringView text) noexcept;
Result<::Aero::Nullable<bool>> ConvertNullableBoolean(
    StringView text) noexcept;
Result<double> ConvertDouble(
    StringView text) noexcept;
Result<String> ConvertString(
    StringView text) noexcept;
Result<Base::ResourceUri> ConvertResourceUri(
    StringView text) noexcept;

template<class T>
Result<T> ConvertInteger(
    StringView text) noexcept {
    static_assert(
        std::is_integral_v<T> &&
        !std::is_same_v<T, bool>);
    String buffer;
    Result<void> assigned =
        buffer.Assign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    if constexpr (std::is_signed_v<T>) {
        const long long value =
            std::strtoll(buffer.CStr(), &end, 10);
        if (end == buffer.CStr() || *end != '\0' ||
            errno == ERANGE ||
            value < static_cast<long long>(
                std::numeric_limits<T>::min()) ||
            value > static_cast<long long>(
                std::numeric_limits<T>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text is not a compatible signed integer");
        }
        return static_cast<T>(value);
    } else {
        const unsigned long long value =
            std::strtoull(buffer.CStr(), &end, 10);
        if (end == buffer.CStr() || *end != '\0' ||
            errno == ERANGE ||
            value > static_cast<unsigned long long>(
                std::numeric_limits<T>::max()) ||
            (!buffer.Empty() && buffer.View()[0] == '-')) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text is not a compatible unsigned integer");
        }
        return static_cast<T>(value);
    }
}

} // namespace ValueConversion

namespace Validate {

template<class T>
bool Finite(const T& value) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return std::isfinite(value);
    } else {
        return true;
    }
}

template<class T>
bool NonNegative(const T& value) noexcept {
    return Finite(value) && value >= T{0};
}

template<class T>
bool Positive(const T& value) noexcept {
    return Finite(value) && value > T{0};
}

} // namespace Validate

} // namespace Aero::Base
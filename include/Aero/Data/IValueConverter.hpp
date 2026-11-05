#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace Aero::Data {

class AERO_GUI_API IValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IValueConverter, Base::Object)

public:
    ~IValueConverter() override = default;

    virtual Result<Value> Convert(const Value& value, const Value& parameter) noexcept = 0;
    virtual Result<Value> ConvertBack(const Value& value, const Value& parameter) noexcept = 0;

protected:
    IValueConverter() noexcept = default;
};

template<class TFrom, class TTo>
class TypedValueConverter : public IValueConverter {
public:
    ~TypedValueConverter() override = default;

    Result<Value> Convert(
        const Value& value,
        const Value& parameter) noexcept override {
        Result<TFrom> decoded = DecodeValue<TFrom>(value);
        if (!decoded) return decoded.GetStatus();
        Result<TTo> converted = ConvertTyped(
            decoded.Value(), parameter, StringView{});
        if (!converted) return converted.GetStatus();
        return Meta::ValueCodec<TTo>::Encode(converted.Value());
    }

    Result<Value> ConvertBack(
        const Value& value,
        const Value& parameter) noexcept override {
        Result<TTo> decoded = DecodeValue<TTo>(value);
        if (!decoded) return decoded.GetStatus();
        Result<TFrom> converted = ConvertBackTyped(
            decoded.Value(), parameter, StringView{});
        if (!converted) return converted.GetStatus();
        return Meta::ValueCodec<TFrom>::Encode(converted.Value());
    }

    virtual Result<TTo> ConvertTyped(
        const TFrom& value,
        const Value& parameter,
        StringView culture = {}) noexcept = 0;

    virtual Result<TFrom> ConvertBackTyped(
        const TTo& /*value*/,
        const Value& /*parameter*/,
        StringView /*culture*/ = {}) noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "ConvertBackTyped is not supported by this converter");
    }

    template<class T>
    static Result<T> DecodeValue(const Value& value) noexcept {
        Result<T> decoded = Meta::ValueCodec<T>::Decode(value);
        if (decoded) return decoded;

        if constexpr (std::is_same_v<T, bool>) {
            switch (value.Kind()) {
            case Meta::ValueKind::Boolean:
                return value.AsBoolean();
            case Meta::ValueKind::SignedInteger:
                return value.AsSignedInteger() != 0;
            case Meta::ValueKind::UnsignedInteger:
                return value.AsUnsignedInteger() != 0U;
            case Meta::ValueKind::Double:
                return value.AsDouble() != 0.0;
            case Meta::ValueKind::String: {
                const StringView s = TrimAscii(value.AsString());
                if (s.SizeBytes() == 4U && EqualsInsensitive(s, "true")) return true;
                if (s.SizeBytes() == 5U && EqualsInsensitive(s, "false")) return false;
                if (s.SizeBytes() == 1U && s[0] == '1') return true;
                if (s.SizeBytes() == 1U && s[0] == '0') return false;
                break;
            }
            default:
                break;
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            switch (value.Kind()) {
            case Meta::ValueKind::Double:
                return static_cast<T>(value.AsDouble());
            case Meta::ValueKind::SignedInteger:
                return static_cast<T>(value.AsSignedInteger());
            case Meta::ValueKind::UnsignedInteger:
                return static_cast<T>(value.AsUnsignedInteger());
            case Meta::ValueKind::Boolean:
                return value.AsBoolean() ? static_cast<T>(1) : static_cast<T>(0);
            case Meta::ValueKind::String: {
                double parsed = 0.0;
                if (TryParseDouble(value.AsString(), parsed)) {
                    return static_cast<T>(parsed);
                }
                break;
            }
            default:
                break;
            }
        } else if constexpr (std::is_integral_v<T>) {
            switch (value.Kind()) {
            case Meta::ValueKind::SignedInteger: {
                const std::int64_t s = value.AsSignedInteger();
                if constexpr (std::is_signed_v<T>) {
                    if (s >= static_cast<std::int64_t>(std::numeric_limits<T>::min()) &&
                        s <= static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
                        return static_cast<T>(s);
                    }
                } else {
                    if (s >= 0 &&
                        static_cast<std::uint64_t>(s) <= static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
                        return static_cast<T>(s);
                    }
                }
                break;
            }
            case Meta::ValueKind::UnsignedInteger: {
                const std::uint64_t u = value.AsUnsignedInteger();
                if (u <= static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
                    return static_cast<T>(u);
                }
                break;
            }
            case Meta::ValueKind::Double: {
                const double d = value.AsDouble();
                if (std::isfinite(d) &&
                    d >= static_cast<double>(std::numeric_limits<T>::min()) &&
                    d <= static_cast<double>(std::numeric_limits<T>::max())) {
                    return static_cast<T>(d);
                }
                break;
            }
            case Meta::ValueKind::Boolean:
                return value.AsBoolean() ? static_cast<T>(1) : static_cast<T>(0);
            case Meta::ValueKind::String: {
                T parsed{};
                if (TryParseIntegral<T>(value.AsString(), parsed)) {
                    return parsed;
                }
                break;
            }
            default:
                break;
            }
        }
        return decoded.GetStatus();
    }

protected:
    TypedValueConverter() noexcept = default;

private:
    static StringView TrimAscii(StringView text) noexcept {
        std::uint32_t begin = 0U;
        std::uint32_t end = text.SizeBytes();
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
            --end;
        }
        return text.Substr(begin, end - begin);
    }

    static bool EqualsInsensitive(StringView a, const char* b) noexcept {
        for (std::uint32_t i = 0U; i < a.SizeBytes(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    static bool TryParseDouble(StringView text, double& out) noexcept {
        const StringView trimmed = TrimAscii(text);
        if (trimmed.Empty() || trimmed.SizeBytes() >= 120U) return false;
        char buffer[128]{};
        for (std::uint32_t i = 0U; i < trimmed.SizeBytes(); ++i) {
            buffer[i] = trimmed[i];
        }
        buffer[trimmed.SizeBytes()] = '\0';
        errno = 0;
        char* endPtr = nullptr;
        const double value = std::strtod(buffer, &endPtr);
        if (endPtr != buffer + trimmed.SizeBytes() || errno == ERANGE || !std::isfinite(value)) {
            return false;
        }
        out = value;
        return true;
    }

    template<class TInt>
    static bool TryParseIntegral(StringView text, TInt& out) noexcept {
        const StringView trimmed = TrimAscii(text);
        if (trimmed.Empty() || trimmed.SizeBytes() >= 64U) return false;
        char buffer[72]{};
        for (std::uint32_t i = 0U; i < trimmed.SizeBytes(); ++i) {
            buffer[i] = trimmed[i];
        }
        buffer[trimmed.SizeBytes()] = '\0';
        errno = 0;
        char* endPtr = nullptr;
        if constexpr (std::is_signed_v<TInt>) {
            const long long value = std::strtoll(buffer, &endPtr, 10);
            if (endPtr != buffer + trimmed.SizeBytes() || errno == ERANGE) {
                return false;
            }
            if (value < static_cast<long long>(std::numeric_limits<TInt>::min()) ||
                value > static_cast<long long>(std::numeric_limits<TInt>::max())) {
                return false;
            }
            out = static_cast<TInt>(value);
            return true;
        } else {
            if (trimmed[0] == '-') return false;
            const unsigned long long value = std::strtoull(buffer, &endPtr, 10);
            if (endPtr != buffer + trimmed.SizeBytes() || errno == ERANGE) {
                return false;
            }
            if (value > static_cast<unsigned long long>(std::numeric_limits<TInt>::max())) {
                return false;
            }
            out = static_cast<TInt>(value);
            return true;
        }
    }
};

} // namespace Aero::Data



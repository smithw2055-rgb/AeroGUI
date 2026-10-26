#include "gui/core/ValueConversion.hpp"

#include <Aero/Base/String.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace Aero::Base::ValueConversion {

Base::StringView Trim(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1U]))) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool EqualsAsciiInsensitive(
    Base::StringView left,
    Base::StringView right) noexcept {
    return left.EqualsInsensitive(right);
}

Base::Result<double> ParseDouble(
    Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.Assign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double value =
        std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() || *end != '\0' ||
        !std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Text is not a finite number");
    }
    return value;
}

Base::Result<bool> ConvertBoolean(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    if (value.SizeBytes() == 4U && EqualsAsciiInsensitive(value, "true")) {
        return true;
    }
    if (value.SizeBytes() == 5U && EqualsAsciiInsensitive(value, "false")) {
        return false;
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<::Aero::Nullable<bool>> ConvertNullableBoolean(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    if (value.Empty() || (value.SizeBytes() == 4U && EqualsAsciiInsensitive(value, "null"))) {
        return ::Aero::Nullable<bool>{};
    }
    Base::Result<bool> converted = ConvertBoolean(value);
    if (!converted) return converted.GetStatus();
    return ::Aero::Nullable<bool>{converted.Value()};
}

Base::Result<double> ConvertDouble(
    Base::StringView text) noexcept {
    return ParseDouble(text);
}

Base::Result<float> ConvertFloat(
    Base::StringView text) noexcept {
    Base::Result<double> parsed = ParseDouble(text);
    if (!parsed) return parsed.GetStatus();
    return static_cast<float>(parsed.Value());
}

Base::Result<Base::String> ConvertString(
    Base::StringView text) noexcept {
    Base::String value;
    Base::Result<void> assigned =
        value.Assign(text);
    if (!assigned) return assigned.GetStatus();
    return value;
}

Base::Result<Base::ResourceUri> ConvertResourceUri(
    Base::StringView text) noexcept {
    return Base::ResourceUri::Parse(Trim(text));
}

Base::Result<void> FormatScalarValue(
    Base::String& output,
    const Meta::Value& value) noexcept {
    switch (value.Kind()) {
    case Meta::ValueKind::String:
        return output.Assign(value.AsString());
    case Meta::ValueKind::Boolean:
        return output.Assign(value.AsBoolean()
            ? Base::StringView("True")
            : Base::StringView("False"));
    case Meta::ValueKind::SignedInteger: {
        char buffer[48]{};
        const int length = std::snprintf(
            buffer, sizeof(buffer), "%lld",
            static_cast<long long>(value.AsSignedInteger()));
        if (length <= 0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Integer formatting failed");
        }
        return output.Assign(Base::StringView(
            buffer, static_cast<std::uint32_t>(length)));
    }
    case Meta::ValueKind::UnsignedInteger: {
        char buffer[48]{};
        const int length = std::snprintf(
            buffer, sizeof(buffer), "%llu",
            static_cast<unsigned long long>(value.AsUnsignedInteger()));
        if (length <= 0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Unsigned integer formatting failed");
        }
        return output.Assign(Base::StringView(
            buffer, static_cast<std::uint32_t>(length)));
    }
    case Meta::ValueKind::Double: {
        char buffer[64]{};
        const int length = std::snprintf(
            buffer, sizeof(buffer), "%.15g", value.AsDouble());
        if (length <= 0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Floating point formatting failed");
        }
        return output.Assign(Base::StringView(
            buffer, static_cast<std::uint32_t>(length)));
    }
    default:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Value has no scalar text representation");
    }
}

Base::Result<Base::String> FormatScalarValue(
    const Meta::Value& value) noexcept {
    Base::String result;
    Base::Result<void> formatted = FormatScalarValue(result, value);
    if (!formatted) return formatted.GetStatus();
    return result;
}

} // namespace Aero::Base::ValueConversion


#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace Aero::Tests {

inline Base::StringView TrimTestText(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    const auto whitespace = [](char character) noexcept {
        return character == ' ' || character == '\t' ||
            character == '\r' || character == '\n';
    };
    while (begin < end && whitespace(value[begin])) ++begin;
    while (end > begin && whitespace(value[end - 1U])) --end;
    return value.Substr(begin, end - begin);
}

inline bool EqualsTestText(
    Base::StringView value,
    const char* literal) noexcept {
    std::uint32_t size = 0U;
    while (literal[size] != '\0') ++size;
    if (value.SizeBytes() != size) return false;
    for (std::uint32_t index = 0U; index < size; ++index) {
        char character = value[index];
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
        if (character != literal[index]) return false;
    }
    return true;
}

inline Base::Result<Core::Value> ConvertTestString(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    return Core::Value::TryFromString(type, text);
}

inline Base::Result<Core::Value> ConvertTestBoolean(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView value = TrimTestText(text);
    if (EqualsTestText(value, "true")) {
        return Core::Value::FromBoolean(type, true);
    }
    if (EqualsTestText(value, "false")) {
        return Core::Value::FromBoolean(type, false);
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Boolean test metadata expects true or false");
}

inline Base::Result<Core::Value> ConvertTestSignedInteger(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(TrimTestText(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const long long value = std::strtoll(buffer.CStr(), &end, 10);
    if (end == buffer.CStr() || *end != '\0' || errno == ERANGE) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Signed-integer test metadata contains invalid text");
    }
    return Core::Value::FromSignedInteger(
        type, static_cast<std::int64_t>(value));
}

inline Base::Result<Core::Value> ConvertTestUnsignedInteger(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView trimmed = TrimTestText(text);
    if (!trimmed.Empty() && trimmed[0] == '-') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Unsigned-integer test metadata cannot be negative");
    }
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(trimmed);
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(buffer.CStr(), &end, 10);
    if (end == buffer.CStr() || *end != '\0' || errno == ERANGE) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Unsigned-integer test metadata contains invalid text");
    }
    return Core::Value::FromUnsignedInteger(
        type, static_cast<std::uint64_t>(value));
}

inline Base::Result<Core::Value> ConvertTestDouble(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(TrimTestText(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() || *end != '\0' || errno == ERANGE ||
        !std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Double test metadata contains invalid text");
    }
    return Core::Value::FromDouble(type, value);
}

inline Base::Result<void> RegisterTestTextConverter(
    Core::MetaRegistrationContext& context,
    Core::TypeId type,
    Core::TextValueConverterCallback converter) noexcept {
    return context.Values().TryRegisterTextConverter(
        {type, converter, nullptr});
}

} // namespace Aero::Tests

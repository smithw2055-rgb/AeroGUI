#include <Aero/Meta/ValueConversion.hpp>

#include <cctype>

namespace Aero::Core::ValueConversion {

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
    if (left.SizeBytes() != right.SizeBytes()) return false;
    for (std::uint32_t index = 0U;
         index < left.SizeBytes(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

Base::Result<double> ParseDouble(
    Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.TryAssign(Trim(text));
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
    if (EqualsAsciiInsensitive(value, "true")) {
        return true;
    }
    if (EqualsAsciiInsensitive(value, "false")) {
        return false;
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<double> ConvertDouble(
    Base::StringView text) noexcept {
    return ParseDouble(text);
}

Base::Result<Base::String> ConvertString(
    Base::StringView text) noexcept {
    Base::String value;
    Base::Result<void> assigned =
        value.TryAssign(text);
    if (!assigned) return assigned.GetStatus();
    return value;
}

Base::Result<Base::ResourceUri> ConvertResourceUri(
    Base::StringView text) noexcept {
    return Base::ResourceUri::Parse(Trim(text));
}

} // namespace Aero::Core::ValueConversion

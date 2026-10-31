#include "gui/core/ValueConversion.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Controls/GridLength.hpp>

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

namespace {

int HexDigit(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

} // namespace

Base::Result<Base::Color> ConvertColor(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    struct NamedColor {
        Base::StringView name;
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::uint8_t alpha;
    };
    static constexpr NamedColor named[] = {
        {"AliceBlue", 240, 248, 255, 255},
        {"Aquamarine", 127, 255, 212, 255},
        {"Black", 0, 0, 0, 255},
        {"Blue", 0, 0, 255, 255},
        {"BurlyWood", 222, 184, 135, 255},
        {"CadetBlue", 95, 158, 160, 255},
        {"Cyan", 0, 255, 255, 255},
        {"DarkBlue", 0, 0, 139, 255},
        {"DarkGray", 169, 169, 169, 255},
        {"DarkOrange", 255, 140, 0, 255},
        {"DimGray", 105, 105, 105, 255},
        {"DodgerBlue", 30, 144, 255, 255},
        {"Gainsboro", 220, 220, 220, 255},
        {"GhostWhite", 248, 248, 255, 255},
        {"Gold", 255, 215, 0, 255},
        {"Gray", 128, 128, 128, 255},
        {"Green", 0, 128, 0, 255},
        {"GreenYellow", 173, 255, 47, 255},
        {"Indigo", 75, 0, 130, 255},
        {"LightBlue", 173, 216, 230, 255},
        {"LightGray", 211, 211, 211, 255},
        {"LightGreen", 144, 238, 144, 255},
        {"LightSeaGreen", 32, 178, 170, 255},
        {"LightSkyBlue", 135, 206, 250, 255},
        {"LightSlateGray", 119, 136, 153, 255},
        {"Lime", 0, 255, 0, 255},
        {"Magenta", 255, 0, 255, 255},
        {"Moccasin", 255, 228, 181, 255},
        {"Orange", 255, 165, 0, 255},
        {"OrangeRed", 255, 69, 0, 255},
        {"Orchid", 218, 112, 214, 255},
        {"PaleTurquoise", 175, 238, 238, 255},
        {"Purple", 128, 0, 128, 255},
        {"Red", 255, 0, 0, 255},
        {"Salmon", 250, 128, 114, 255},
        {"Silver", 192, 192, 192, 255},
        {"SkyBlue", 135, 206, 235, 255},
        {"SteelBlue", 70, 130, 180, 255},
        {"Teal", 0, 128, 128, 255},
        {"Thistle", 216, 191, 216, 255},
        {"Tomato", 255, 99, 71, 255},
        {"Transparent", 255, 255, 255, 0},
        {"Turquoise", 64, 224, 208, 255},
        {"White", 255, 255, 255, 255},
        {"WhiteSmoke", 245, 245, 245, 255},
        {"Yellow", 255, 255, 0, 255},
        {"YellowGreen", 154, 205, 50, 255}};
    for (const NamedColor& candidate : named) {
        if (!EqualsAsciiInsensitive(value, candidate.name)) {
            continue;
        }
        return Base::Color{
            candidate.red / 255.0F,
            candidate.green / 255.0F,
            candidate.blue / 255.0F,
            candidate.alpha / 255.0F};
    }
    if ((value.SizeBytes() != 4U && value.SizeBytes() != 5U &&
         value.SizeBytes() != 7U && value.SizeBytes() != 9U) ||
        value[0] != '#') {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Color requires #RGB, #ARGB, #RRGGBB, or #AARRGGBB");
    }
    if (value.SizeBytes() == 4U || value.SizeBytes() == 5U) {
        const bool alpha = value.SizeBytes() == 5U;
        std::uint8_t components[4]{255U, 0U, 0U, 0U};
        const std::uint32_t count = alpha ? 4U : 3U;
        for (std::uint32_t index = 0U; index < count; ++index) {
            const int digit = HexDigit(value[1U + index]);
            if (digit < 0) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Color contains a non-hex digit");
            }
            components[index] = static_cast<std::uint8_t>(
                (digit << 4) | digit);
        }
        return alpha
            ? Base::Color{
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  components[3] / 255.0F,
                  components[0] / 255.0F}
            : Base::Color{
                  components[0] / 255.0F,
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  1.0F};
    }
    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t count = value.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const int high = HexDigit(value[1U + index * 2U]);
        const int low = HexDigit(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return count == 3U
        ? Base::Color{bytes[0] / 255.0F, bytes[1] / 255.0F, bytes[2] / 255.0F, 1.0F}
        : Base::Color{bytes[1] / 255.0F, bytes[2] / 255.0F,
            bytes[3] / 255.0F, bytes[0] / 255.0F};
}

Base::Result<Base::Thickness> ConvertThickness(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    std::uint32_t count = 0U;
    bool valid = true;
    while (*cursor != '\0') {
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (count == 4U) {
            valid = false;
            break;
        }
        char* end = nullptr;
        values[count] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[count])) {
            valid = false;
            break;
        }
        ++count;
        cursor = end;
        const char* whitespace = cursor;
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (*cursor == ',') {
            ++cursor;
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            if (*cursor == '\0') {
                valid = false;
                break;
            }
        } else if (cursor == whitespace) {
            valid = false;
            break;
        }
    }
    Base::Thickness result;
    if (!valid) return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness contains invalid text");
    if (count == 1U) result = {values[0], values[0], values[0], values[0]};
    else if (count == 2U) result = {values[0], values[1], values[0], values[1]};
    else if (count == 4U) result = {values[0], values[1], values[2], values[3]};
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness accepts one, two, or four numbers");
    return result;
}

Base::Result<Base::CornerRadius> ConvertCornerRadius(
    Base::StringView text) noexcept {
    Base::Result<Base::Thickness> parsed = ConvertThickness(text);
    if (!parsed) return parsed.GetStatus();
    const Base::Thickness& values = parsed.Value();
    return Base::CornerRadius{
        values.left,
        values.top,
        values.right,
        values.bottom};
}

Base::Result<Base::Point> ConvertPoint(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    char* end = nullptr;
    const double x = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(x)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point requires two finite coordinates");
    }
    cursor = end;
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    const double y = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(y)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point requires two finite coordinates");
    }
    cursor = end;
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point contains trailing text");
    }
    return Base::Point{x, y};
}

Base::Result<Base::Transform2D> ConvertMatrix(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[6]{};
    for (std::uint32_t index = 0U; index < 6U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Matrix requires six finite values");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Matrix contains trailing text");
    }
    return Base::Transform2D{
        values[0], values[1], values[2],
        values[3], values[4], values[5]};
}

Base::Result<::Aero::Length> ConvertLength(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    ::Aero::Length length = ::Aero::Length::Auto();
    if (!EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed = ParseDouble(value);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = ::Aero::Length::Pixels(parsed.Value());
    }
    return length;
}

Base::Result<::Aero::GridLength> ConvertGridLength(
    Base::StringView text) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "auto")) {
        return ::Aero::GridLength::Auto();
    }
    if (!value.Empty() && value[value.SizeBytes() - 1U] == '*') {
        const Base::StringView weightText =
            Trim(value.Substr(0U, value.SizeBytes() - 1U));
        double weight = 1.0;
        if (!weightText.Empty()) {
            Base::Result<double> parsed = ParseDouble(weightText);
            if (!parsed) return parsed.GetStatus();
            weight = parsed.Value();
        }
        if (!std::isfinite(weight) || weight <= 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "GridLength star weight must be positive and finite");
        }
        return ::Aero::GridLength::Star(weight);
    }
    Base::Result<double> pixels = ParseDouble(value);
    if (!pixels || pixels.Value() < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "GridLength must be Auto, a nonnegative pixel value, or a star weight");
    }
    return ::Aero::GridLength::Pixel(pixels.Value());
}

} // namespace Aero::Base::ValueConversion


// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::MetadataSupport {
using namespace ::Aero::Media;
namespace {

Base::Result<Length> ConvertLength(
    Base::StringView text) noexcept {
    const Base::StringView value = ::Aero::Base::ValueConversion::Trim(text);
    Length length = Length::Auto();
    if (!::Aero::Base::ValueConversion::EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ParseDouble(value);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = Length::Pixels(parsed.Value());
    }
    return length;
}

Base::Result<Thickness> ParseThickness(Base::StringView input) noexcept {
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
    Thickness result;
    if (!valid) return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness contains invalid text");
    if (count == 1U) result = {values[0], values[0], values[0], values[0]};
    else if (count == 2U) result = {values[0], values[1], values[0], values[1]};
    else if (count == 4U) result = {values[0], values[1], values[2], values[3]};
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness accepts one, two, or four numbers");
    return result;
}

Base::Result<Thickness> ConvertThickness(
    Base::StringView text) noexcept {
    return ParseThickness(text);
}

Base::Result<CornerRadius> ConvertCornerRadius(
    Base::StringView text) noexcept {
    Base::Result<Thickness> parsed =
        ParseThickness(text);
    if (!parsed) return parsed.GetStatus();
    const Thickness& values = parsed.Value();
    return CornerRadius{
        values.left,
        values.top,
        values.right,
        values.bottom};
}

Base::Result<Point> ConvertPoint(
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
    return Point{x, y};
}

Base::Result<Base::Size> ConvertSize(
    Base::StringView input) noexcept {
    Base::Result<Point> parsed = ConvertPoint(input);
    if (!parsed) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Size requires two finite values");
    }
    return Base::Size{parsed.Value().x, parsed.Value().y};
}

Base::Result<Rect> ConvertRect(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned =
        text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U;
         index < 4U; ++index) {
        while (*cursor == ' ' ||
               *cursor == ',') {
            ++cursor;
        }
        char* end = nullptr;
        values[index] =
            std::strtod(cursor, &end);
        if (end == cursor ||
            !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Rect requires four finite values");
        }
        cursor = end;
    }
    while (std::isspace(
        static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor != '\0' ||
        values[2] < 0.0 ||
        values[3] < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Rect requires x,y,width,height with non-negative size");
    }
    return Rect{
        values[0], values[1],
        values[2], values[3]};
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

Base::Result<Base::Transform3> ConvertTransform3(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[12]{};
    for (std::uint32_t index = 0U; index < 12U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Transform3 requires twelve finite values");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Transform3 contains trailing text");
    }
    return Base::Transform3{
        values[0], values[1], values[2],
        values[3], values[4], values[5],
        values[6], values[7], values[8],
        values[9], values[10], values[11]};
}

int Hex(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Color> ConvertColor(
    Base::StringView text) noexcept {
    const Base::StringView value = ::Aero::Base::ValueConversion::Trim(text);
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
        if (!::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
                value, candidate.name)) {
            continue;
        }
        return Color{
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
            const int digit = Hex(value[1U + index]);
            if (digit < 0) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Color contains a non-hex digit");
            }
            components[index] = static_cast<std::uint8_t>(
                (digit << 4) | digit);
        }
        return alpha
            ? Color{
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  components[3] / 255.0F,
                  components[0] / 255.0F}
            : Color{
                  components[0] / 255.0F,
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  1.0F};
    }
    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t count = value.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const int high = Hex(value[1U + index * 2U]);
        const int low = Hex(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    Color color = count == 3U
        ? Color{bytes[0] / 255.0F, bytes[1] / 255.0F, bytes[2] / 255.0F, 1.0F}
        : Color{bytes[1] / 255.0F, bytes[2] / 255.0F,
            bytes[3] / 255.0F, bytes[0] / 255.0F};
    return color;
}

bool EqualLength(const void* left, const void* right, void*) noexcept {
    const Length& a = *static_cast<const Length*>(left);
    const Length& b = *static_cast<const Length*>(right);
    return a.isAuto == b.isAuto && (a.isAuto || a.value == b.value);
}

bool EqualThickness(const void* left, const void* right, void*) noexcept {
    const Thickness& a = *static_cast<const Thickness*>(left);
    const Thickness& b = *static_cast<const Thickness*>(right);
    return a.left == b.left && a.top == b.top &&
        a.right == b.right && a.bottom == b.bottom;
}

bool EqualColor(const void* left, const void* right, void*) noexcept {
    const Color& a = *static_cast<const Color*>(left);
    const Color& b = *static_cast<const Color*>(right);
    return a.red == b.red && a.green == b.green &&
        a.blue == b.blue && a.alpha == b.alpha;
}

bool EqualCornerRadius(
    const void* left,
    const void* right,
    void*) noexcept {
    const CornerRadius& a =
        *static_cast<const CornerRadius*>(left);
    const CornerRadius& b =
        *static_cast<const CornerRadius*>(right);
    return a.topLeft == b.topLeft &&
        a.topRight == b.topRight &&
        a.bottomRight == b.bottomRight &&
        a.bottomLeft == b.bottomLeft;
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero {

AERO_DESCRIBE(Length) {
    using namespace Aero::Meta;
    Register<Length>(context)
            .ValueSemantics({sizeof(Length), alignof(Length), nullptr, nullptr, &::Aero::MetadataSupport::EqualLength, nullptr, true})
            .TextConverter<&::Aero::MetadataSupport::ConvertLength>();
}

} // namespace Aero


namespace Aero {
using namespace ::Aero::MetadataSupport;
using namespace ::Aero::Meta;
using namespace ::Aero::Input;

Base::Result<void> PopulateUiMedia(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status;
    ::Aero::Meta::DescribeHook<::Aero::Length>::Run(context);

    Register<Thickness>(context)
        .Field<&Thickness::left>("Left")
        .Field<&Thickness::top>("Top")
        .Field<&Thickness::right>("Right")
        .Field<&Thickness::bottom>("Bottom")
        .ValueSemantics({sizeof(Thickness), alignof(Thickness), nullptr, nullptr, &::Aero::MetadataSupport::EqualThickness, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertThickness>();

    Register<CornerRadius>(context)
        .Field<&CornerRadius::topLeft>("TopLeft")
        .Field<&CornerRadius::topRight>("TopRight")
        .Field<&CornerRadius::bottomRight>("BottomRight")
        .Field<&CornerRadius::bottomLeft>("BottomLeft")
        .ValueSemantics({ sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr, &::Aero::MetadataSupport::EqualCornerRadius, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertCornerRadius>();

    Register<Color>(context)
        .Field<&Color::red>("Red")
        .Field<&Color::green>("Green")
        .Field<&Color::blue>("Blue")
        .Field<&Color::alpha>("Alpha")
        .ValueSemantics({sizeof(Color), alignof(Color), nullptr, nullptr, &::Aero::MetadataSupport::EqualColor, nullptr, true})
        .TextConverter<&::Aero::MetadataSupport::ConvertColor>();

    Register<Point>(context)
        .Field<&Point::x>("X")
        .Field<&Point::y>("Y")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertPoint>();

    Register<Rect>(context)
        .Field<&Rect::x>("X")
        .Field<&Rect::y>("Y")
        .Field<&Rect::width>("Width")
        .Field<&Rect::height>("Height")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertRect>();

    Register<Base::Size>(context)
        .Field<&Base::Size::width>("Width")
        .Field<&Base::Size::height>("Height")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertSize>();

    ::Aero::Meta::DescribeHook<::Aero::Animatable>::Run(context);

    // Brush.RelativeTransform is a Transform-valued dependency property, so
    // the abstract value type must exist before Brush metadata is authored.
    ::Aero::Meta::DescribeHook<::Aero::Media::Transform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Brush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::SolidColorBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::GradientStop>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::GradientStopCollection>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::BrushShader>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::MonochromeShader>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::ConicGradientShader>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::WavesShader>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::GradientBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::LinearGradientBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::RadialGradientBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::ImageSource>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::TileBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::BitmapImage>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::CroppedBitmap>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::ImageBrush>::Run(context);

    Register<Base::Transform2D>(context)
        .Field<&Base::Transform2D::m11>("M11")
        .Field<&Base::Transform2D::m12>("M12")
        .Field<&Base::Transform2D::m21>("M21")
        .Field<&Base::Transform2D::m22>("M22")
        .Field<&Base::Transform2D::dx>("OffsetX")
        .Field<&Base::Transform2D::dy>("OffsetY")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertMatrix>();

    Register<Base::Transform3>(context)
        .Field<&Base::Transform3::m11>("M11")
        .Field<&Base::Transform3::m12>("M12")
        .Field<&Base::Transform3::m13>("M13")
        .Field<&Base::Transform3::m21>("M21")
        .Field<&Base::Transform3::m22>("M22")
        .Field<&Base::Transform3::m23>("M23")
        .Field<&Base::Transform3::m31>("M31")
        .Field<&Base::Transform3::m32>("M32")
        .Field<&Base::Transform3::m33>("M33")
        .Field<&Base::Transform3::dx>("OffsetX")
        .Field<&Base::Transform3::dy>("OffsetY")
        .Field<&Base::Transform3::dz>("OffsetZ")
        .ValueSemantics()
        .TextConverter<&::Aero::MetadataSupport::ConvertTransform3>();

    ::Aero::Meta::DescribeHook<::Aero::Media::TranslateTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::ScaleTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::RotateTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::SkewTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::MatrixTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::CompositeTransform>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::TransformGroup>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Transform3D>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::CompositeTransform3D>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::PerspectiveTransform3D>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::MatrixTransform3D>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::Effect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::BlurEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::DropShadowEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::PixelateEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::TintEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::DirectionalBlurEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::ShaderEffect>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::MediaElement>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Media::VisualBrush>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Input::ICommand>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::InputGesture>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::KeyGesture>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::RoutedCommand>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::RoutedUICommand>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::InputBinding>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::CommandBinding>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Input::ApplicationCommands>::Run(context);
    status = ApplicationCommands::RegisterDefaults();
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero

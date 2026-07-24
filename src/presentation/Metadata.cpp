#include <Aero/Presentation/Metadata.hpp>

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

constexpr double DefaultMaximum = 1.0e12;

bool EqualsAsciiInsensitive(Base::StringView value, const char* literal) noexcept {
    std::uint32_t size = 0U;
    while (literal[size] != '\0') ++size;
    if (value.SizeBytes() != size) return false;
    for (std::uint32_t index = 0U; index < size; ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(literal[index]))) return false;
    }
    return true;
}

Base::StringView Trim(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1U]))) --end;
    return value.Substr(begin, end - begin);
}

Base::Result<double> ParseDouble(Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double value = std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() || *end != '\0' || !std::isfinite(value)) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Text is not a finite number");
    }
    return value;
}

Base::Result<Value> ConvertLength(TypeId type, Base::StringView text,
    void* context) noexcept {
    auto* values = static_cast<MetadataValueRegistrationStore*>(context);
    const Base::StringView value = Trim(text);
    Length length = Length::Auto();
    if (!EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed = ParseDouble(value);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = Length::Pixels(parsed.Value());
    }
    return MetadataRegistrationValues(*values).TryCreateValue(type, &length);
}

Base::Result<Thickness> ParseThickness(Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(input);
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

Base::Result<Value> ConvertThickness(TypeId type, Base::StringView text,
    void* context) noexcept {
    Base::Result<Thickness> parsed = ParseThickness(text);
    if (!parsed) return parsed.GetStatus();
    return MetadataRegistrationValues(
        *static_cast<MetadataValueRegistrationStore*>(context)).TryCreateValue(
            type, &parsed.Value());
}

int Hex(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Value> ConvertColor(TypeId type, Base::StringView text,
    void* context) noexcept {
    const Base::StringView value = Trim(text);
    if ((value.SizeBytes() != 7U && value.SizeBytes() != 9U) || value[0] != '#') {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Color requires #RRGGBB or #AARRGGBB");
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
    return MetadataRegistrationValues(
        *static_cast<MetadataValueRegistrationStore*>(context)).TryCreateValue(
            type, &color);
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

Base::Result<Value> ConvertHorizontal(TypeId type, Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    HorizontalAlignment result;
    if (EqualsAsciiInsensitive(value, "stretch")) result = HorizontalAlignment::Stretch;
    else if (EqualsAsciiInsensitive(value, "left")) result = HorizontalAlignment::Left;
    else if (EqualsAsciiInsensitive(value, "center")) result = HorizontalAlignment::Center;
    else if (EqualsAsciiInsensitive(value, "right")) result = HorizontalAlignment::Right;
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "HorizontalAlignment is invalid");
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(result));
}
Base::Result<Value> ConvertVertical(TypeId type, Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    VerticalAlignment result;
    if (EqualsAsciiInsensitive(value, "stretch")) result = VerticalAlignment::Stretch;
    else if (EqualsAsciiInsensitive(value, "top")) result = VerticalAlignment::Top;
    else if (EqualsAsciiInsensitive(value, "center")) result = VerticalAlignment::Center;
    else if (EqualsAsciiInsensitive(value, "bottom")) result = VerticalAlignment::Bottom;
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "VerticalAlignment is invalid");
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(result));
}
bool ValidateLength(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Length& length = *static_cast<const Length*>(value.AsCustom());
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateNonnegativeDouble(const Value& value) noexcept {
    return value.Kind() == ValueKind::Double &&
        std::isfinite(value.AsDouble()) && value.AsDouble() >= 0.0;
}
bool ValidateThicknessValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Thickness& t = *static_cast<const Thickness*>(value.AsCustom());
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
}
bool ValidateHorizontalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(HorizontalAlignment::Right);
}
bool ValidateVerticalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(VerticalAlignment::Bottom);
}
Base::Result<Value> CheckMinimum(DependencyObject& object,
    const Value& value, DependencyPropertyHandle maximum) noexcept {
    Base::Result<Value> other = object.GetValue(maximum);
    if (!other || value.AsDouble() > other.Value().AsDouble()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Minimum layout size exceeds maximum layout size");
    }
    return value;
}
Base::Result<Value> CheckMaximum(DependencyObject& object,
    const Value& value, DependencyPropertyHandle minimum) noexcept {
    Base::Result<Value> other = object.GetValue(minimum);
    if (!other || value.AsDouble() < other.Value().AsDouble()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Maximum layout size is below minimum layout size");
    }
    return value;
}
Base::Result<Value> CoerceMinWidth(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMinimum(o, v, FrameworkElement::MaxWidthProperty); }
Base::Result<Value> CoerceMaxWidth(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, FrameworkElement::MinWidthProperty); }
Base::Result<Value> CoerceMinHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMinimum(o, v, FrameworkElement::MaxHeightProperty); }
Base::Result<Value> CoerceMaxHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, FrameworkElement::MinHeightProperty); }

} // namespace

Base::Result<void> Detail::PopulatePresentationMetadata(
    Core::MetaRegistrationContext& context) noexcept {
    Base::Result<void> status;

    MetaTypeBuilder<EventArgs> eventArgs =
        MetaTypeBuilder<EventArgs>::Struct(context);
    status = eventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<RoutedEventArgs> routedEventArgs =
        MetaTypeBuilder<RoutedEventArgs>::Struct(context);
    status = routedEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<InputEventArgs> inputEventArgs =
        MetaTypeBuilder<InputEventArgs>::Struct(context);
    status = inputEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<MouseEventArgs> mouseEventArgs =
        MetaTypeBuilder<MouseEventArgs>::Struct(context);
    status = mouseEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<MouseButtonEventArgs> mouseButtonEventArgs =
        MetaTypeBuilder<MouseButtonEventArgs>::Struct(context);
    status = mouseButtonEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<KeyEventArgs> keyEventArgs =
        MetaTypeBuilder<KeyEventArgs>::Struct(context);
    status = keyEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<TextCompositionEventArgs> textCompositionEventArgs =
        MetaTypeBuilder<TextCompositionEventArgs>::Struct(context);
    status = textCompositionEventArgs.Finish();
    if (!status) return status.GetStatus();
    MetaTypeBuilder<KeyboardFocusChangedEventArgs> focusEventArgs =
        MetaTypeBuilder<KeyboardFocusChangedEventArgs>::Struct(context);
    status = focusEventArgs.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Length> length =
        MetaTypeBuilder<Length>::Struct(context);
    length
        .ValueSemantics({sizeof(Length), alignof(Length), nullptr, nullptr,
            &EqualLength, nullptr, true})
        .TextConverter(&ConvertLength, &context.ValueRegistrations());
    status = length.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Thickness> thickness =
        MetaTypeBuilder<Thickness>::Struct(context);
    thickness
        .Field<&Thickness::left>("Left")
        .Field<&Thickness::top>("Top")
        .Field<&Thickness::right>("Right")
        .Field<&Thickness::bottom>("Bottom")
        .ValueSemantics({sizeof(Thickness), alignof(Thickness), nullptr,
            nullptr, &EqualThickness, nullptr, true})
        .TextConverter(&ConvertThickness, &context.ValueRegistrations());
    status = thickness.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Color> color = MetaTypeBuilder<Color>::Struct(context);
    color
        .Field<&Color::red>("Red")
        .Field<&Color::green>("Green")
        .Field<&Color::blue>("Blue")
        .Field<&Color::alpha>("Alpha")
        .ValueSemantics({sizeof(Color), alignof(Color), nullptr, nullptr,
            &EqualColor, nullptr, true})
        .TextConverter(&ConvertColor, &context.ValueRegistrations());
    status = color.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<HorizontalAlignment> horizontal =
        MetaTypeBuilder<HorizontalAlignment>::Enum(
            context, TypeOf<std::uint32_t>());
    horizontal
        .EnumValue("Stretch", HorizontalAlignment::Stretch)
        .EnumValue("Left", HorizontalAlignment::Left)
        .EnumValue("Center", HorizontalAlignment::Center)
        .EnumValue("Right", HorizontalAlignment::Right)
        .TextConverter(&ConvertHorizontal);
    status = horizontal.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<VerticalAlignment> vertical =
        MetaTypeBuilder<VerticalAlignment>::Enum(
            context, TypeOf<std::uint32_t>());
    vertical
        .EnumValue("Stretch", VerticalAlignment::Stretch)
        .EnumValue("Top", VerticalAlignment::Top)
        .EnumValue("Center", VerticalAlignment::Center)
        .EnumValue("Bottom", VerticalAlignment::Bottom)
        .TextConverter(&ConvertVertical);
    status = vertical.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Visual> visual =
        MetaTypeBuilder<Visual>::Object(context, TypeFlags::Abstract);
    status = visual.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<UIElement> uiElement =
        MetaTypeBuilder<UIElement>::Object(context, TypeFlags::Abstract);
    if (context.RoutedEvents() != nullptr) {
        uiElement
            .RoutedEvent(UIElement::MouseMoveEvent, "MouseMove",
                TypeOf<MouseEventArgs>(), RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::MouseDownEvent, "MouseDown",
                TypeOf<MouseButtonEventArgs>(), RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::MouseUpEvent, "MouseUp",
                TypeOf<MouseButtonEventArgs>(), RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::GotKeyboardFocusEvent,
                "GotKeyboardFocus", TypeOf<KeyboardFocusChangedEventArgs>(),
                RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::LostKeyboardFocusEvent,
                "LostKeyboardFocus", TypeOf<KeyboardFocusChangedEventArgs>(),
                RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::KeyDownEvent, "KeyDown",
                TypeOf<KeyEventArgs>(), RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::KeyUpEvent, "KeyUp",
                TypeOf<KeyEventArgs>(), RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::TextInputEvent, "TextInput",
                TypeOf<TextCompositionEventArgs>(),
                RoutingStrategy::Bubble);
    }
    uiElement
        .DependencyProperty(UIElement::ClipToBoundsProperty, "ClipToBounds",
            TypeOf<bool>(),
            Value::FromBoolean(TypeOf<bool>(), false),
            PropertyMetadataFlags::AffectsArrange)
        .DependencyProperty(UIElement::IsHitTestVisibleProperty,
            "IsHitTestVisible", TypeOf<bool>(),
            Value::FromBoolean(TypeOf<bool>(), true),
            PropertyMetadataFlags::None);
    status = uiElement.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<FrameworkElement> frameworkElement =
        MetaTypeBuilder<FrameworkElement>::Object(
            context, TypeFlags::Abstract);
    const Length autoSource = Length::Auto();
    Base::Result<Value> automatic = context.Values().TryCreateValue(
        TypeOf<Length>(), &autoSource);
    if (!automatic) return automatic.GetStatus();
    const Thickness zero{};
    Base::Result<Value> margin = context.Values().TryCreateValue(
        TypeOf<Thickness>(), &zero);
    if (!margin) return margin.GetStatus();
    frameworkElement
        .DependencyProperty(FrameworkElement::DataContextProperty,
            "DataContext", TypeOf<Base::Object>(),
            Value::NullObject(TypeOf<Base::Object>()),
            PropertyMetadataFlags::Inherits)
        .DependencyProperty(FrameworkElement::WidthProperty, "Width",
            TypeOf<Length>(), automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateLength)
        .DependencyProperty(FrameworkElement::HeightProperty, "Height",
            TypeOf<Length>(), automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateLength)
        .DependencyProperty(FrameworkElement::MinWidthProperty, "MinWidth",
            TypeOf<double>(),
            Value::FromDouble(TypeOf<double>(), 0.0),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMinWidth)
        .DependencyProperty(FrameworkElement::MaxWidthProperty, "MaxWidth",
            TypeOf<double>(),
            Value::FromDouble(TypeOf<double>(), DefaultMaximum),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMaxWidth)
        .DependencyProperty(FrameworkElement::MinHeightProperty, "MinHeight",
            TypeOf<double>(),
            Value::FromDouble(TypeOf<double>(), 0.0),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMinHeight)
        .DependencyProperty(FrameworkElement::MaxHeightProperty, "MaxHeight",
            TypeOf<double>(),
            Value::FromDouble(TypeOf<double>(), DefaultMaximum),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMaxHeight)
        .DependencyProperty(FrameworkElement::MarginProperty, "Margin",
            TypeOf<Thickness>(), margin.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateThicknessValue)
        .DependencyProperty(FrameworkElement::HorizontalAlignmentProperty,
            "HorizontalAlignment", TypeOf<HorizontalAlignment>(),
            Value::FromUnsignedInteger(
                TypeOf<HorizontalAlignment>(), 0U),
            PropertyMetadataFlags::AffectsArrange, &ValidateHorizontalValue)
        .DependencyProperty(FrameworkElement::VerticalAlignmentProperty,
            "VerticalAlignment", TypeOf<VerticalAlignment>(),
            Value::FromUnsignedInteger(
                TypeOf<VerticalAlignment>(), 0U),
            PropertyMetadataFlags::AffectsArrange, &ValidateVerticalValue)
        .DependencyProperty(FrameworkElement::UseLayoutRoundingProperty,
            "UseLayoutRounding", TypeOf<bool>(),
            Value::FromBoolean(TypeOf<bool>(), false),
            PropertyMetadataFlags::AffectsMeasure);
    status = frameworkElement.Finish();
    return status;
}

} // namespace Aero::Presentation

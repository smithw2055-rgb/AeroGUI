#include <Aero/Core/Presentation.hpp>

#include <Aero/Core/Controls.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/MetadataDsl.hpp>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Aero::Core {
namespace {

constexpr double DefaultMaximum = 1.0e12;
thread_local PresentationContext* CurrentPresentationContext = nullptr;

struct DefaultPresentationRuntime final {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore behaviors{types};
    MetadataValueRegistrationStore values{types};
    DependencyPropertyRegistry properties{types, behaviors};
    RoutedEventRegistry routedEvents{types, behaviors};
    bool ready = false;

    DefaultPresentationRuntime() noexcept {
        MetaRegistrationContext context(
            types, behaviors, values, properties, &routedEvents);
        Base::Result<void> registered =
            TryRegisterPresentationMetadata(context);
        if (!registered || !types.Freeze() || !behaviors.Freeze() ||
            !values.Freeze() || !properties.Freeze() ||
            !routedEvents.Freeze()) {
            return;
        }
        ready = true;
    }
};

DefaultPresentationRuntime& GetDefaultPresentationRuntime() noexcept {
    thread_local DefaultPresentationRuntime runtime;
    AERO_ASSERT(runtime.ready);
    return runtime;
}

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

Base::Result<Value> ConvertBoolean(TypeId type, Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "true")) return Value::FromBoolean(type, true);
    if (EqualsAsciiInsensitive(value, "false")) return Value::FromBoolean(type, false);
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<Value> ConvertUnsigned(TypeId type, Base::StringView text,
    void*) noexcept {
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(buffer.CStr(), &end, 10);
    if (end == buffer.CStr() || *end != '\0' || errno == ERANGE ||
        value > static_cast<unsigned long long>(UINT32_MAX) ||
        (!buffer.Empty() && buffer.View()[0] == '-')) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Text is not an unsigned integer");
    }
    return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(value));
}

Base::Result<Value> ConvertDoubleValue(TypeId type, Base::StringView text,
    void*) noexcept {
    Base::Result<double> value = ParseDouble(text);
    return value ? Base::Result<Value>(Value::FromDouble(type, value.Value()))
                 : Base::Result<Value>(value.GetStatus());
}

Base::Result<Value> ConvertString(TypeId type, Base::StringView text,
    void*) noexcept {
    return Value::TryFromString(type, text);
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
Base::Result<Value> ConvertOrientation(TypeId type, Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "horizontal"))
        return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(Orientation::Horizontal));
    if (EqualsAsciiInsensitive(value, "vertical"))
        return Value::FromUnsignedInteger(type, static_cast<std::uint64_t>(Orientation::Vertical));
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Orientation is invalid");
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
bool ValidateFiniteDouble(const Value& value) noexcept {
    return value.Kind() == ValueKind::Double && std::isfinite(value.AsDouble());
}
bool ValidateThicknessValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Thickness& t = *static_cast<const Thickness*>(value.AsCustom());
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
}
bool ValidateColorValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Color& color = *static_cast<const Color*>(value.AsCustom());
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}
bool ValidateHorizontalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(HorizontalAlignment::Right);
}
bool ValidateVerticalValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(VerticalAlignment::Bottom);
}
bool ValidateOrientationValue(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= static_cast<std::uint64_t>(Orientation::Vertical);
}
bool ValidateUInt32(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <= std::numeric_limits<std::uint32_t>::max();
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

PresentationContext GetCurrentPresentationContext() noexcept {
    if (CurrentPresentationContext != nullptr) {
        return *CurrentPresentationContext;
    }
    DefaultPresentationRuntime& runtime = GetDefaultPresentationRuntime();
    return {&runtime.dispatcher, &runtime.properties, &runtime.values, nullptr};
}

Base::Result<Value> TryCreatePresentationValue(
    TypeId type,
    const void* source) noexcept {
    PresentationContext context = GetCurrentPresentationContext();
    if (context.metadataRuntime != nullptr) {
        return context.metadataRuntime->TryCreateValue(type, source);
    }
    if (context.valueRegistrations != nullptr) {
        return MetadataRegistrationValues(
            *context.valueRegistrations).TryCreateValue(type, source);
    }
    DefaultPresentationRuntime& fallback = GetDefaultPresentationRuntime();
    return MetadataRegistrationValues(fallback.values).TryCreateValue(
        type, source);
}

PresentationContextScope::PresentationContextScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties) noexcept
    : context_{&dispatcher, &properties, nullptr, nullptr},
      previous_(CurrentPresentationContext),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentPresentationContext = &context_;
}

PresentationContextScope::PresentationContextScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    MetadataValueRegistrationStore& values) noexcept
    : context_{&dispatcher, &properties, &values, nullptr},
      previous_(CurrentPresentationContext),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentPresentationContext = &context_;
}

PresentationContextScope::PresentationContextScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    MetadataRuntime& runtime) noexcept
    : context_{&dispatcher, &properties, nullptr, &runtime},
      previous_(CurrentPresentationContext),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentPresentationContext = &context_;
}

PresentationContextScope::PresentationContextScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    MetadataRuntime* runtime) noexcept
    : context_{&dispatcher, &properties, nullptr, runtime},
      previous_(CurrentPresentationContext),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentPresentationContext = &context_;
}

PresentationContextScope::~PresentationContextScope() {
    AERO_ASSERT(ownerThread_ == CurrentDispatcherThreadToken());
    AERO_ASSERT(CurrentPresentationContext == &context_);
    CurrentPresentationContext = previous_;
}

Base::Result<void> TryRegisterPresentationMetadata(
    MetaRegistrationContext& context) noexcept {
    Base::Result<void> status;

    MetaTypeBuilder<Base::Object> object =
        MetaTypeBuilder<Base::Object>::Object(context);
    status = object.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<bool> boolean =
        MetaTypeBuilder<bool>::Primitive(context);
    boolean.TextConverter(&ConvertBoolean);
    status = boolean.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<std::uint32_t> unsignedInteger =
        MetaTypeBuilder<std::uint32_t>::Primitive(context);
    unsignedInteger.TextConverter(&ConvertUnsigned);
    status = unsignedInteger.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<double> number =
        MetaTypeBuilder<double>::Primitive(context);
    number.TextConverter(&ConvertDoubleValue);
    status = number.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Base::String> string =
        MetaTypeBuilder<Base::String>::Primitive(context);
    string.TextConverter(&ConvertString);
    status = string.Finish();
    if (!status) return status.GetStatus();

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
            context, BuiltinTypes::UnsignedInteger);
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
            context, BuiltinTypes::UnsignedInteger);
    vertical
        .EnumValue("Stretch", VerticalAlignment::Stretch)
        .EnumValue("Top", VerticalAlignment::Top)
        .EnumValue("Center", VerticalAlignment::Center)
        .EnumValue("Bottom", VerticalAlignment::Bottom)
        .TextConverter(&ConvertVertical);
    status = vertical.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Orientation> orientation =
        MetaTypeBuilder<Orientation>::Enum(
            context, BuiltinTypes::UnsignedInteger);
    orientation
        .EnumValue("Horizontal", Orientation::Horizontal)
        .EnumValue("Vertical", Orientation::Vertical)
        .TextConverter(&ConvertOrientation);
    status = orientation.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<DependencyObject> dependencyObject =
        MetaTypeBuilder<DependencyObject>::Object(
            context, TypeFlags::Abstract);
    status = dependencyObject.Finish();
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
                BuiltinTypes::MouseEventArgs, RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::MouseDownEvent, "MouseDown",
                BuiltinTypes::MouseButtonEventArgs, RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::MouseUpEvent, "MouseUp",
                BuiltinTypes::MouseButtonEventArgs, RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::GotKeyboardFocusEvent,
                "GotKeyboardFocus", BuiltinTypes::KeyboardFocusChangedEventArgs,
                RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::LostKeyboardFocusEvent,
                "LostKeyboardFocus", BuiltinTypes::KeyboardFocusChangedEventArgs,
                RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::KeyDownEvent, "KeyDown",
                BuiltinTypes::KeyEventArgs, RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::KeyUpEvent, "KeyUp",
                BuiltinTypes::KeyEventArgs, RoutingStrategy::Bubble)
            .RoutedEvent(UIElement::TextInputEvent, "TextInput",
                BuiltinTypes::TextCompositionEventArgs,
                RoutingStrategy::Bubble);
    }
    uiElement
        .DependencyProperty(UIElement::ClipToBoundsProperty, "ClipToBounds",
            BuiltinTypes::Boolean,
            Value::FromBoolean(BuiltinTypes::Boolean, false),
            PropertyMetadataFlags::AffectsArrange)
        .DependencyProperty(UIElement::IsHitTestVisibleProperty,
            "IsHitTestVisible", BuiltinTypes::Boolean,
            Value::FromBoolean(BuiltinTypes::Boolean, true),
            PropertyMetadataFlags::None);
    status = uiElement.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<FrameworkElement> frameworkElement =
        MetaTypeBuilder<FrameworkElement>::Object(
            context, TypeFlags::Abstract);
    const Length autoSource = Length::Auto();
    Base::Result<Value> automatic = context.Values().TryCreateValue(
        BuiltinTypes::Length, &autoSource);
    if (!automatic) return automatic.GetStatus();
    const Thickness zero{};
    Base::Result<Value> margin = context.Values().TryCreateValue(
        BuiltinTypes::Thickness, &zero);
    if (!margin) return margin.GetStatus();
    frameworkElement
        .DependencyProperty(FrameworkElement::WidthProperty, "Width",
            BuiltinTypes::Length, automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateLength)
        .DependencyProperty(FrameworkElement::HeightProperty, "Height",
            BuiltinTypes::Length, automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateLength)
        .DependencyProperty(FrameworkElement::MinWidthProperty, "MinWidth",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, 0.0),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMinWidth)
        .DependencyProperty(FrameworkElement::MaxWidthProperty, "MaxWidth",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, DefaultMaximum),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMaxWidth)
        .DependencyProperty(FrameworkElement::MinHeightProperty, "MinHeight",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, 0.0),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMinHeight)
        .DependencyProperty(FrameworkElement::MaxHeightProperty, "MaxHeight",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, DefaultMaximum),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble, &CoerceMaxHeight)
        .DependencyProperty(FrameworkElement::MarginProperty, "Margin",
            BuiltinTypes::Thickness, margin.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateThicknessValue)
        .DependencyProperty(FrameworkElement::HorizontalAlignmentProperty,
            "HorizontalAlignment", BuiltinTypes::HorizontalAlignment,
            Value::FromUnsignedInteger(
                BuiltinTypes::HorizontalAlignment, 0U),
            PropertyMetadataFlags::AffectsArrange, &ValidateHorizontalValue)
        .DependencyProperty(FrameworkElement::VerticalAlignmentProperty,
            "VerticalAlignment", BuiltinTypes::VerticalAlignment,
            Value::FromUnsignedInteger(
                BuiltinTypes::VerticalAlignment, 0U),
            PropertyMetadataFlags::AffectsArrange, &ValidateVerticalValue)
        .DependencyProperty(FrameworkElement::UseLayoutRoundingProperty,
            "UseLayoutRounding", BuiltinTypes::Boolean,
            Value::FromBoolean(BuiltinTypes::Boolean, false),
            PropertyMetadataFlags::AffectsMeasure);
    status = frameworkElement.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Panel> panel =
        MetaTypeBuilder<Panel>::Object(context, TypeFlags::Abstract);
    panel.Content("Children", ContentKind::Collection);
    status = panel.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Decorator> decorator =
        MetaTypeBuilder<Decorator>::Object(context, TypeFlags::Abstract);
    decorator.Content("Content", ContentKind::Single);
    status = decorator.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Control> control =
        MetaTypeBuilder<Control>::Object(context, TypeFlags::Abstract);
    status = control.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ContentControl> contentControl =
        MetaTypeBuilder<ContentControl>::Object(
            context, TypeFlags::Abstract);
    contentControl.Content("Content", ContentKind::Single);
    status = contentControl.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<UserControl> userControl =
        MetaTypeBuilder<UserControl>::Object(context);
    status = userControl.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<StackPanel> stackPanel =
        MetaTypeBuilder<StackPanel>::Object(context);
    stackPanel
        .DependencyProperty(StackPanel::OrientationProperty, "Orientation",
            BuiltinTypes::Orientation,
            Value::FromUnsignedInteger(BuiltinTypes::Orientation, 1U),
            PropertyMetadataFlags::AffectsMeasure, &ValidateOrientationValue)
        .Content("Children", ContentKind::Collection);
    status = stackPanel.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Canvas> canvas =
        MetaTypeBuilder<Canvas>::Object(context);
    canvas
        .AttachedDependencyProperty(Canvas::LeftProperty, "Left",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, 0.0),
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .AttachedDependencyProperty(Canvas::TopProperty, "Top",
            BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, 0.0),
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .Content("Children", ContentKind::Collection);
    status = canvas.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Grid> grid = MetaTypeBuilder<Grid>::Object(context);
    grid
        .AttachedDependencyProperty(Grid::RowProperty, "Row",
            BuiltinTypes::UnsignedInteger,
            Value::FromUnsignedInteger(BuiltinTypes::UnsignedInteger, 0U),
            PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32)
        .AttachedDependencyProperty(Grid::ColumnProperty, "Column",
            BuiltinTypes::UnsignedInteger,
            Value::FromUnsignedInteger(BuiltinTypes::UnsignedInteger, 0U),
            PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32)
        .Content("Children", ContentKind::Collection);
    status = grid.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Border> border = MetaTypeBuilder<Border>::Object(context);
    const Color transparent{};
    Base::Result<Value> transparentValue = context.Values().TryCreateValue(
        BuiltinTypes::Color, &transparent);
    if (!transparentValue) return transparentValue.GetStatus();
    Base::Result<Value> padding = context.Values().TryCreateValue(
        BuiltinTypes::Thickness, &zero);
    if (!padding) return padding.GetStatus();
    border
        .DependencyProperty(Border::BackgroundProperty, "Background",
            BuiltinTypes::Color, transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue)
        .DependencyProperty(Border::BorderBrushProperty, "BorderBrush",
            BuiltinTypes::Color, transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue)
        .DependencyProperty(Border::BorderThicknessProperty,
            "BorderThickness", BuiltinTypes::Double,
            Value::FromDouble(BuiltinTypes::Double, 0.0),
            PropertyMetadataFlags::AffectsRender,
            &ValidateNonnegativeDouble)
        .DependencyProperty(Border::PaddingProperty, "Padding",
            BuiltinTypes::Thickness, padding.Value(),
            PropertyMetadataFlags::AffectsMeasure, &ValidateThicknessValue)
        .Content("Content", ContentKind::Single);
    status = border.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<TextBlock> textBlock =
        MetaTypeBuilder<TextBlock>::Object(context);
    Base::Result<Value> text = Value::TryFromString(BuiltinTypes::String, {});
    if (!text) return text.GetStatus();
    const Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> foreground = context.Values().TryCreateValue(
        BuiltinTypes::Color, &black);
    if (!foreground) return foreground.GetStatus();
    textBlock
        .DependencyProperty(TextBlock::TextProperty, "Text",
            BuiltinTypes::String, text.Value(),
            PropertyMetadataFlags::AffectsMeasure)
        .DependencyProperty(TextBlock::ForegroundProperty, "Foreground",
            BuiltinTypes::Color, foreground.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    status = textBlock.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ContentPresenter> contentPresenter =
        MetaTypeBuilder<ContentPresenter>::Object(context);
    contentPresenter.Content("Content", ContentKind::Single);
    return contentPresenter.Finish();
}

} // namespace Aero::Core

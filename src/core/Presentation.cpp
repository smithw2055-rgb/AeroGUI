#include <Aero/Core/Presentation.hpp>

#include <Aero/Core/Controls.hpp>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Aero::Core {
namespace {

constexpr Base::StringView PresentationNamespace = AeroNamespaceUri();
constexpr double DefaultMaximum = 1.0e12;
thread_local PresentationContext* CurrentPresentationContext = nullptr;

struct DefaultPresentationRuntime final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    RoutedEventRegistry routedEvents{types};
    bool ready = false;

    DefaultPresentationRuntime() noexcept {
        Base::Result<void> registered =
            TryRegisterPresentationMetadata(
                types, properties, &routedEvents);
        if (!registered || !types.Freeze() || !properties.Freeze() ||
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
    auto* types = static_cast<TypeRegistry*>(context);
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
    return types->TryCreateValue(type, &length);
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
    return static_cast<TypeRegistry*>(context)->TryCreateValue(
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
    return static_cast<TypeRegistry*>(context)->TryCreateValue(
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

Base::Result<TypeId> RegisterType(TypeRegistry& types, Base::StringView name,
    TypeId base, TypeFlags flags = TypeFlags::None) noexcept {
    return types.TryRegisterType({PresentationNamespace, name, base, flags, nullptr});
}

} // namespace

PresentationContext GetCurrentPresentationContext() noexcept {
    if (CurrentPresentationContext != nullptr) {
        return *CurrentPresentationContext;
    }
    DefaultPresentationRuntime& runtime = GetDefaultPresentationRuntime();
    return {&runtime.dispatcher, &runtime.properties};
}

PresentationContextScope::PresentationContextScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties) noexcept
    : context_{&dispatcher, &properties},
      previous_(CurrentPresentationContext),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentPresentationContext = &context_;
}

PresentationContextScope::~PresentationContextScope() {
    AERO_ASSERT(ownerThread_ == CurrentDispatcherThreadToken());
    AERO_ASSERT(CurrentPresentationContext == &context_);
    CurrentPresentationContext = previous_;
}

AERO_IMPLEMENT_EMPTY_METADATA(DependencyObject, TypeFlags::Abstract)
AERO_IMPLEMENT_EMPTY_METADATA(Visual, TypeFlags::Abstract)

AERO_IMPLEMENT_METADATA(UIElement, TypeFlags::Abstract) {
    if (helper.Context().routedEvents != nullptr) {
        AeroEvent(MouseMove, BuiltinTypes::MouseEventArgs, RoutingStrategy::Bubble);
        AeroEvent(MouseDown, BuiltinTypes::MouseButtonEventArgs, RoutingStrategy::Bubble);
        AeroEvent(MouseUp, BuiltinTypes::MouseButtonEventArgs, RoutingStrategy::Bubble);
        AeroEvent(GotKeyboardFocus, BuiltinTypes::KeyboardFocusChangedEventArgs,
            RoutingStrategy::Bubble);
        AeroEvent(LostKeyboardFocus, BuiltinTypes::KeyboardFocusChangedEventArgs,
            RoutingStrategy::Bubble);
        AeroEvent(KeyDown, BuiltinTypes::KeyEventArgs, RoutingStrategy::Bubble);
        AeroEvent(KeyUp, BuiltinTypes::KeyEventArgs, RoutingStrategy::Bubble);
        AeroEvent(TextInput, BuiltinTypes::TextCompositionEventArgs,
            RoutingStrategy::Bubble);
    }
    const auto arrange = PropertyMetadataFlags::AffectsArrange;
    AeroDP(ClipToBounds, BuiltinTypes::Boolean,
        Value::FromBoolean(BuiltinTypes::Boolean, false), arrange);
    AeroDP(IsHitTestVisible, BuiltinTypes::Boolean,
        Value::FromBoolean(BuiltinTypes::Boolean, true),
        PropertyMetadataFlags::None);
}

AERO_IMPLEMENT_METADATA(FrameworkElement, TypeFlags::Abstract) {
    MetaRegistrationContext& context = helper.Context();
    const Length autoSource = Length::Auto();
    Base::Result<Value> automatic = context.types.TryCreateValue(
        BuiltinTypes::Length, &autoSource);
    if (!automatic) {
        helper.Fail(automatic.GetStatus());
        return;
    }
    const Thickness zero{};
    Base::Result<Value> margin = context.types.TryCreateValue(
        BuiltinTypes::Thickness, &zero);
    if (!margin) {
        helper.Fail(margin.GetStatus());
        return;
    }
    const auto measure = PropertyMetadataFlags::AffectsMeasure;
    const auto arrange = PropertyMetadataFlags::AffectsArrange;
    AeroDP(Width, BuiltinTypes::Length, automatic.Value(), measure,
        &ValidateLength);
    AeroDP(Height, BuiltinTypes::Length, automatic.Value(), measure,
        &ValidateLength);
    AeroDP(MinWidth, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinWidth);
    AeroDP(MaxWidth, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxWidth);
    AeroDP(MinHeight, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinHeight);
    AeroDP(MaxHeight, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxHeight);
    AeroDP(Margin, BuiltinTypes::Thickness, margin.Value(), measure,
        &ValidateThicknessValue);
    AeroDP(HorizontalAlignment, BuiltinTypes::HorizontalAlignment,
        Value::FromUnsignedInteger(BuiltinTypes::HorizontalAlignment, 0U),
        arrange, &ValidateHorizontalValue);
    AeroDP(VerticalAlignment, BuiltinTypes::VerticalAlignment,
        Value::FromUnsignedInteger(BuiltinTypes::VerticalAlignment, 0U),
        arrange, &ValidateVerticalValue);
    AeroDP(UseLayoutRounding, BuiltinTypes::Boolean,
        Value::FromBoolean(BuiltinTypes::Boolean, false), measure);
}


AERO_IMPLEMENT_METADATA(StackPanel, TypeFlags::None) {
    AeroDP(Orientation, BuiltinTypes::Orientation,
        Value::FromUnsignedInteger(BuiltinTypes::Orientation, 1U),
        PropertyMetadataFlags::AffectsMeasure, &ValidateOrientationValue);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Canvas, TypeFlags::None) {
    AeroAttachedDP(Left, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble);
    AeroAttachedDP(Top, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Grid, TypeFlags::None) {
    AeroAttachedDP(Row, BuiltinTypes::UnsignedInteger,
        Value::FromUnsignedInteger(BuiltinTypes::UnsignedInteger, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32);
    AeroAttachedDP(Column, BuiltinTypes::UnsignedInteger,
        Value::FromUnsignedInteger(BuiltinTypes::UnsignedInteger, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Border, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    const Color transparent{};
    Base::Result<Value> color = context.types.TryCreateValue(
        BuiltinTypes::Color, &transparent);
    if (!color) {
        helper.Fail(color.GetStatus());
        return;
    }
    const Thickness zero{};
    Base::Result<Value> padding = context.types.TryCreateValue(
        BuiltinTypes::Thickness, &zero);
    if (!padding) {
        helper.Fail(padding.GetStatus());
        return;
    }
    AeroDP(Background, BuiltinTypes::Color, color.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    AeroDP(BorderBrush, BuiltinTypes::Color, color.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    AeroDP(BorderThickness, BuiltinTypes::Double,
        Value::FromDouble(BuiltinTypes::Double, 0.0),
        PropertyMetadataFlags::AffectsRender, &ValidateNonnegativeDouble);
    AeroDP(Padding, BuiltinTypes::Thickness, padding.Value(),
        PropertyMetadataFlags::AffectsMeasure, &ValidateThicknessValue);
    AeroContent("Content", ContentKind::Single);
}

AERO_IMPLEMENT_METADATA(TextBlock, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    Base::Result<Value> text = Value::TryFromString(
        BuiltinTypes::String, {});
    if (!text) {
        helper.Fail(text.GetStatus());
        return;
    }
    const Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> foreground = context.types.TryCreateValue(
        BuiltinTypes::Color, &black);
    if (!foreground) {
        helper.Fail(foreground.GetStatus());
        return;
    }
    AeroDP(Text, BuiltinTypes::String, text.Value(),
        PropertyMetadataFlags::AffectsMeasure);
    AeroDP(Foreground, BuiltinTypes::Color, foreground.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
}

AERO_IMPLEMENT_METADATA(ContentPresenter, TypeFlags::None) {
    AeroContent("Content", ContentKind::Single);
}

Base::Result<void> TryRegisterPresentationMetadata(
    TypeRegistry& types,
    DependencyPropertyRegistry& properties,
    RoutedEventRegistry* routedEvents) noexcept {
    auto registerType = [&types](Base::StringView name, TypeId base,
        TypeFlags flags, TypeId expected) noexcept -> Base::Result<void> {
        Base::Result<TypeId> registered = RegisterType(types, name, base, flags);
        if (!registered) return registered.GetStatus();
        if (registered.Value() != expected) {
            return Base::Status::Failure(Base::ErrorCode::InternalError,
                "Built-in metadata type ID does not match its canonical ID");
        }
        return {};
    };

    Base::Result<void> status = registerType(
        Base::StringView("Object"), InvalidTypeId, TypeFlags::None,
        BuiltinTypes::Object);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("EventArgs"), InvalidTypeId,
        TypeFlags::ValueType, BuiltinTypes::EventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("RoutedEventArgs"),
        BuiltinTypes::EventArgs, TypeFlags::ValueType,
        BuiltinTypes::RoutedEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("InputEventArgs"),
        BuiltinTypes::RoutedEventArgs, TypeFlags::ValueType,
        BuiltinTypes::InputEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("MouseEventArgs"),
        BuiltinTypes::InputEventArgs, TypeFlags::ValueType,
        BuiltinTypes::MouseEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("MouseButtonEventArgs"),
        BuiltinTypes::MouseEventArgs, TypeFlags::ValueType,
        BuiltinTypes::MouseButtonEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("KeyEventArgs"),
        BuiltinTypes::InputEventArgs, TypeFlags::ValueType,
        BuiltinTypes::KeyEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("TextCompositionEventArgs"),
        BuiltinTypes::InputEventArgs, TypeFlags::ValueType,
        BuiltinTypes::TextCompositionEventArgs);
    if (!status) return status.GetStatus();
    status = registerType(Base::StringView("KeyboardFocusChangedEventArgs"),
        BuiltinTypes::RoutedEventArgs, TypeFlags::ValueType,
        BuiltinTypes::KeyboardFocusChangedEventArgs);
    if (!status) return status.GetStatus();

    const struct BuiltinValueType final {
        Base::StringView name;
        TypeId id;
    } valueTypes[] = {
        {Base::StringView("Boolean"), BuiltinTypes::Boolean},
        {Base::StringView("UInt32"), BuiltinTypes::UnsignedInteger},
        {Base::StringView("Double"), BuiltinTypes::Double},
        {Base::StringView("String"), BuiltinTypes::String},
        {Base::StringView("Length"), BuiltinTypes::Length},
        {Base::StringView("Thickness"), BuiltinTypes::Thickness},
        {Base::StringView("Color"), BuiltinTypes::Color},
        {Base::StringView("HorizontalAlignment"),
            BuiltinTypes::HorizontalAlignment},
        {Base::StringView("VerticalAlignment"),
            BuiltinTypes::VerticalAlignment},
        {Base::StringView("Orientation"), BuiltinTypes::Orientation}
    };
    for (const BuiltinValueType& valueType : valueTypes) {
        status = registerType(valueType.name, InvalidTypeId,
            TypeFlags::ValueType, valueType.id);
        if (!status) return status.GetStatus();
    }

    status = types.TryRegisterValueSemantics(BuiltinTypes::Length,
        {sizeof(Length), alignof(Length), nullptr, nullptr,
         &EqualLength, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(BuiltinTypes::Thickness,
        {sizeof(Thickness), alignof(Thickness), nullptr, nullptr,
         &EqualThickness, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(BuiltinTypes::Color,
        {sizeof(Color), alignof(Color), nullptr, nullptr,
         &EqualColor, nullptr, true});
    if (!status) return status.GetStatus();

    const TextValueConverterRegistration converters[] = {
        {BuiltinTypes::Boolean, &ConvertBoolean, nullptr},
        {BuiltinTypes::UnsignedInteger, &ConvertUnsigned, nullptr},
        {BuiltinTypes::Double, &ConvertDoubleValue, nullptr},
        {BuiltinTypes::String, &ConvertString, nullptr},
        {BuiltinTypes::Length, &ConvertLength, &types},
        {BuiltinTypes::Thickness, &ConvertThickness, &types},
        {BuiltinTypes::Color, &ConvertColor, &types},
        {BuiltinTypes::HorizontalAlignment, &ConvertHorizontal, nullptr},
        {BuiltinTypes::VerticalAlignment, &ConvertVertical, nullptr},
        {BuiltinTypes::Orientation, &ConvertOrientation, nullptr}
    };
    for (const TextValueConverterRegistration& converter : converters) {
        status = types.TryRegisterTextConverter(converter);
        if (!status) return status.GetStatus();
    }

    MetaRegistrationContext registrationContext{
        types, properties, routedEvents};
    using Registrar = Base::Result<void> (*)(MetaRegistrationContext&) noexcept;
    const Registrar presentationRegistrars[] = {
        &DependencyObject::TryRegisterMetadata,
        &Visual::TryRegisterMetadata,
        &UIElement::TryRegisterMetadata,
        &FrameworkElement::TryRegisterMetadata
    };
    for (Registrar registrar : presentationRegistrars) {
        status = registrar(registrationContext);
        if (!status) return status.GetStatus();
    }
    status = TryRegisterControlPrimitiveMetadata(registrationContext);
    if (!status) return status.GetStatus();

    const Registrar controlRegistrars[] = {
        &StackPanel::TryRegisterMetadata,
        &Canvas::TryRegisterMetadata,
        &Grid::TryRegisterMetadata,
        &Border::TryRegisterMetadata,
        &TextBlock::TryRegisterMetadata,
        &ContentPresenter::TryRegisterMetadata
    };
    for (Registrar registrar : controlRegistrars) {
        status = registrar(registrationContext);
        if (!status) return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Core

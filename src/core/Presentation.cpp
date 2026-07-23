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
        Base::Result<CorePresentationMetadata> registered =
            TryRegisterCorePresentationMetadata(
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
    const Value& v) noexcept { return CheckMinimum(o, v, LayoutElement::MaxWidthProperty); }
Base::Result<Value> CoerceMaxWidth(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, LayoutElement::MinWidthProperty); }
Base::Result<Value> CoerceMinHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMinimum(o, v, LayoutElement::MaxHeightProperty); }
Base::Result<Value> CoerceMaxHeight(DependencyObject& o, const DependencyProperty&,
    const Value& v) noexcept { return CheckMaximum(o, v, LayoutElement::MinHeightProperty); }

Base::Result<TypeId> RegisterType(TypeRegistry& types, Base::StringView name,
    TypeId base, TypeFlags flags = TypeFlags::None) noexcept {
    return types.TryRegisterType({PresentationNamespace, name, base, flags, nullptr});
}

} // namespace

Base::StringView AeroPresentationNamespaceUri() noexcept {
    return AeroNamespaceUri();
}

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
AERO_IMPLEMENT_METADATA(TreeNode, TypeFlags::Abstract) {
    if (helper.Context().routedEvents == nullptr) return;
    const CorePresentationMetadata& m = helper.Context().core;
    AeroEvent(MouseMove, m.mouseEventArgsType, RoutingStrategy::Bubble);
    AeroEvent(MouseDown, m.mouseButtonEventArgsType, RoutingStrategy::Bubble);
    AeroEvent(MouseUp, m.mouseButtonEventArgsType, RoutingStrategy::Bubble);
    AeroEvent(GotKeyboardFocus, m.keyboardFocusChangedEventArgsType,
        RoutingStrategy::Bubble);
    AeroEvent(LostKeyboardFocus, m.keyboardFocusChangedEventArgsType,
        RoutingStrategy::Bubble);
    AeroEvent(KeyDown, m.keyEventArgsType, RoutingStrategy::Bubble);
    AeroEvent(KeyUp, m.keyEventArgsType, RoutingStrategy::Bubble);
    AeroEvent(TextInput, m.textCompositionEventArgsType,
        RoutingStrategy::Bubble);
}

AERO_IMPLEMENT_METADATA(LayoutElement, TypeFlags::Abstract) {
    MetaRegistrationContext& context = helper.Context();
    const CorePresentationMetadata& m = context.core;
    const Length autoSource = Length::Auto();
    Base::Result<Value> automatic = context.types.TryCreateValue(
        m.lengthType, &autoSource);
    if (!automatic) {
        helper.Fail(automatic.GetStatus());
        return;
    }
    const Thickness zero{};
    Base::Result<Value> margin = context.types.TryCreateValue(
        m.thicknessType, &zero);
    if (!margin) {
        helper.Fail(margin.GetStatus());
        return;
    }
    const auto measure = PropertyMetadataFlags::AffectsMeasure;
    const auto arrange = PropertyMetadataFlags::AffectsArrange;
    AeroDP(Width, m.lengthType, automatic.Value(), measure, &ValidateLength);
    AeroDP(Height, m.lengthType, automatic.Value(), measure, &ValidateLength);
    AeroDP(MinWidth, m.doubleType,
        Value::FromDouble(m.doubleType, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinWidth);
    AeroDP(MaxWidth, m.doubleType,
        Value::FromDouble(m.doubleType, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxWidth);
    AeroDP(MinHeight, m.doubleType,
        Value::FromDouble(m.doubleType, 0.0), measure,
        &ValidateNonnegativeDouble, &CoerceMinHeight);
    AeroDP(MaxHeight, m.doubleType,
        Value::FromDouble(m.doubleType, DefaultMaximum), measure,
        &ValidateNonnegativeDouble, &CoerceMaxHeight);
    AeroDP(Margin, m.thicknessType, margin.Value(), measure,
        &ValidateThicknessValue);
    AeroDP(HorizontalAlignment, m.horizontalAlignmentType,
        Value::FromUnsignedInteger(m.horizontalAlignmentType, 0U), arrange,
        &ValidateHorizontalValue);
    AeroDP(VerticalAlignment, m.verticalAlignmentType,
        Value::FromUnsignedInteger(m.verticalAlignmentType, 0U), arrange,
        &ValidateVerticalValue);
    AeroDP(ClipToBounds, m.booleanType,
        Value::FromBoolean(m.booleanType, false), arrange);
    AeroDP(IsHitTestVisible, m.booleanType,
        Value::FromBoolean(m.booleanType, true), PropertyMetadataFlags::None);
    AeroDP(UseLayoutRounding, m.booleanType,
        Value::FromBoolean(m.booleanType, false), measure);
}

AERO_IMPLEMENT_EMPTY_METADATA(RenderElement, TypeFlags::Abstract)

AERO_IMPLEMENT_METADATA(StackPanel, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    AeroDP(Orientation, context.core.orientationType,
        Value::FromUnsignedInteger(context.core.orientationType, 1U),
        PropertyMetadataFlags::AffectsMeasure, &ValidateOrientationValue);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Canvas, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    AeroAttachedDP(Left, context.core.doubleType,
        Value::FromDouble(context.core.doubleType, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble);
    AeroAttachedDP(Top, context.core.doubleType,
        Value::FromDouble(context.core.doubleType, 0.0),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateFiniteDouble);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Grid, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    AeroAttachedDP(Row, context.core.unsignedIntegerType,
        Value::FromUnsignedInteger(context.core.unsignedIntegerType, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32);
    AeroAttachedDP(Column, context.core.unsignedIntegerType,
        Value::FromUnsignedInteger(context.core.unsignedIntegerType, 0U),
        PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32);
    AeroContent("Children", ContentKind::Collection);
}

AERO_IMPLEMENT_METADATA(Border, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    const Color transparent{};
    Base::Result<Value> color = context.types.TryCreateValue(
        context.core.colorType, &transparent);
    if (!color) {
        helper.Fail(color.GetStatus());
        return;
    }
    const Thickness zero{};
    Base::Result<Value> padding = context.types.TryCreateValue(
        context.core.thicknessType, &zero);
    if (!padding) {
        helper.Fail(padding.GetStatus());
        return;
    }
    AeroDP(Background, context.core.colorType, color.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    AeroDP(BorderBrush, context.core.colorType, color.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    AeroDP(BorderThickness, context.core.doubleType,
        Value::FromDouble(context.core.doubleType, 0.0),
        PropertyMetadataFlags::AffectsRender, &ValidateNonnegativeDouble);
    AeroDP(Padding, context.core.thicknessType, padding.Value(),
        PropertyMetadataFlags::AffectsMeasure, &ValidateThicknessValue);
    AeroContent("Content", ContentKind::Single);
}

AERO_IMPLEMENT_METADATA(TextBlock, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    Base::Result<Value> text = Value::TryFromString(
        context.core.stringType, {});
    if (!text) {
        helper.Fail(text.GetStatus());
        return;
    }
    const Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> foreground = context.types.TryCreateValue(
        context.core.colorType, &black);
    if (!foreground) {
        helper.Fail(foreground.GetStatus());
        return;
    }
    AeroDP(Text, context.core.stringType, text.Value(),
        PropertyMetadataFlags::AffectsMeasure);
    AeroDP(Foreground, context.core.colorType, foreground.Value(),
        PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
}

AERO_IMPLEMENT_METADATA(ContentPresenter, TypeFlags::None) {
    AeroContent("Content", ContentKind::Single);
}

Base::Result<CorePresentationMetadata> TryRegisterCorePresentationMetadata(
    TypeRegistry& types,
    DependencyPropertyRegistry& properties,
    RoutedEventRegistry* routedEvents) noexcept {
    CorePresentationMetadata m;
#define AERO_REGISTER_TYPE(field, name, base, flags) \
    do { auto r = RegisterType(types, Base::StringView(name), base, flags); \
         if (!r) return r.GetStatus(); m.field = r.Value(); } while (false)
    AERO_REGISTER_TYPE(objectType, "Object", InvalidTypeId, TypeFlags::None);
    AERO_REGISTER_TYPE(eventArgsType, "EventArgs", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(routedEventArgsType, "RoutedEventArgs",
        m.eventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(inputEventArgsType, "InputEventArgs",
        m.routedEventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(mouseEventArgsType, "MouseEventArgs",
        m.inputEventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(mouseButtonEventArgsType, "MouseButtonEventArgs",
        m.mouseEventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(keyEventArgsType, "KeyEventArgs",
        m.inputEventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(textCompositionEventArgsType, "TextCompositionEventArgs",
        m.inputEventArgsType, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(keyboardFocusChangedEventArgsType,
        "KeyboardFocusChangedEventArgs", m.routedEventArgsType,
        TypeFlags::ValueType);
    m.dependencyObjectType = DependencyObject::StaticTypeId();
    m.treeNodeType = TreeNode::StaticTypeId();
    m.layoutElementType = LayoutElement::StaticTypeId();
    m.renderElementType = RenderElement::StaticTypeId();
    m.stackPanelType = StackPanel::StaticTypeId();
    m.canvasType = Canvas::StaticTypeId();
    m.gridType = Grid::StaticTypeId();
    m.borderType = Border::StaticTypeId();
    m.textBlockType = TextBlock::StaticTypeId();
    m.contentPresenterType = ContentPresenter::StaticTypeId();
    AERO_REGISTER_TYPE(booleanType, "Boolean", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(unsignedIntegerType, "UInt32", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(doubleType, "Double", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(stringType, "String", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(lengthType, "Length", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(thicknessType, "Thickness", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(colorType, "Color", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(horizontalAlignmentType, "HorizontalAlignment", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(verticalAlignmentType, "VerticalAlignment", InvalidTypeId, TypeFlags::ValueType);
    AERO_REGISTER_TYPE(orientationType, "Orientation", InvalidTypeId, TypeFlags::ValueType);
#undef AERO_REGISTER_TYPE

    Base::Result<void> status = types.TryRegisterValueSemantics(m.lengthType,
        {sizeof(Length), alignof(Length), nullptr, nullptr, &EqualLength, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(m.thicknessType,
        {sizeof(Thickness), alignof(Thickness), nullptr, nullptr, &EqualThickness, nullptr, true});
    if (!status) return status.GetStatus();
    status = types.TryRegisterValueSemantics(m.colorType,
        {sizeof(Color), alignof(Color), nullptr, nullptr, &EqualColor, nullptr, true});
    if (!status) return status.GetStatus();

    const TextValueConverterRegistration converters[] = {
        {m.booleanType, &ConvertBoolean, nullptr},
        {m.unsignedIntegerType, &ConvertUnsigned, nullptr},
        {m.doubleType, &ConvertDoubleValue, nullptr},
        {m.stringType, &ConvertString, nullptr},
        {m.lengthType, &ConvertLength, &types},
        {m.thicknessType, &ConvertThickness, &types},
        {m.colorType, &ConvertColor, &types},
        {m.horizontalAlignmentType, &ConvertHorizontal, nullptr},
        {m.verticalAlignmentType, &ConvertVertical, nullptr},
        {m.orientationType, &ConvertOrientation, nullptr}
    };
    for (const TextValueConverterRegistration& converter : converters) {
        status = types.TryRegisterTextConverter(converter);
        if (!status) return status.GetStatus();
    }

    MetaRegistrationContext registrationContext{types, properties, m, routedEvents};
    using Registrar = Base::Result<void> (*)(MetaRegistrationContext&) noexcept;
    const Registrar registrars[] = {
        &DependencyObject::TryRegisterMetadata,
        &TreeNode::TryRegisterMetadata,
        &LayoutElement::TryRegisterMetadata,
        &RenderElement::TryRegisterMetadata,
        &StackPanel::TryRegisterMetadata,
        &Canvas::TryRegisterMetadata,
        &Grid::TryRegisterMetadata,
        &Border::TryRegisterMetadata,
        &TextBlock::TryRegisterMetadata,
        &ContentPresenter::TryRegisterMetadata
    };
    for (Registrar registrar : registrars) {
        status = registrar(registrationContext);
        if (!status) return status.GetStatus();
    }

    return m;
}

} // namespace Aero::Core

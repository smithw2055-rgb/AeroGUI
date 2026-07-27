#include <Aero/Presentation/Metadata.hpp>

#include <Aero/Base/Ascii.hpp>

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

constexpr double DefaultMaximum = 1.0e12;

Base::Result<double> ParseDouble(Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned = buffer.TryAssign(Base::TrimAscii(text));
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
    const Base::StringView value = Base::TrimAscii(text);
    Length length = Length::Auto();
    if (!Base::EqualsAsciiInsensitive(value, "auto")) {
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
    const Base::StringView value = Base::TrimAscii(text);
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

bool ValidateLength(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Length& length = *static_cast<const Length*>(value.AsCustom());
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateNonnegativeDouble(const Value& value) noexcept {
    return value.Kind() == ValueKind::Double &&
        std::isfinite(value.AsDouble()) && value.AsDouble() >= 0.0;
}
bool ValidateUInt32(const Value& value) noexcept {
    return value.Kind() == ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            std::numeric_limits<std::uint32_t>::max();
}
bool ValidateThicknessValue(const Value& value) noexcept {
    if (value.Kind() != ValueKind::Custom) return false;
    const Thickness& t = *static_cast<const Thickness*>(value.AsCustom());
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
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

Base::Result<void> SetFrameworkElementResources(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement Resources expects a ResourceDictionary");
    }
    auto& source = static_cast<ResourceDictionary&>(
        *value.AsObject());
    auto& target =
        static_cast<FrameworkElement&>(object).Resources();
    if (target.Size() != 0U ||
        target.MergedDictionaryCount() != 0U ||
        !target.Source().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "FrameworkElement Resources is already assigned");
    }
    target = std::move(source);
    return {};
}

Base::Result<void> SetStyleResources(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Resources expects a ResourceDictionary");
    }
    auto& target = static_cast<Style&>(object).Resources();
    if (target.Size() != 0U ||
        target.MergedDictionaryCount() != 0U ||
        !target.Source().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Style Resources is already assigned");
    }
    auto& source = static_cast<ResourceDictionary&>(
        *value.AsObject());
    target = std::move(source);
    return {};
}

PropertyRegistration OrdinaryProperty(
    Base::StringView name,
    TypeId type,
    PropertyGetCallback get,
    PropertySetCallback set,
    PropertyFlags flags =
        PropertyFlags::None) noexcept {
    PropertyRegistration registration;
    registration.name = name;
    registration.valueType = type;
    registration.flags = flags;
    registration.access =
        PropertyAccessKind::Ordinary;
    registration.get = get;
    registration.set = set;
    return registration;
}

} // namespace

Base::Result<void> Detail::PopulatePresentationMetadata(
    Core::MetaRegistrationContext& context) noexcept {
    Base::Result<void> status;

    auto resourceDictionary = Describe<ResourceDictionary>(context);
    resourceDictionary
        .Property({
            "Source",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property({
            "MergedDictionaries",
            ResourceDictionary::StaticTypeId(),
            PropertyFlags::Structural |
                PropertyFlags::Collection})
        .Content<Base::Object>(
            "Entries",
            ContentKind::Collection)
        .Factory();
    status = resourceDictionary.Finish();
    if (!status) return status.GetStatus();

    auto setter = Describe<Setter>(context);
    setter
        .Property({
            "TargetName",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property({
            "Property",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property({
            "Value",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Factory();
    status = setter.Finish();
    if (!status) return status.GetStatus();

    auto trigger = Describe<PropertyTrigger>(context);
    trigger
        .Property({
            "Property",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property({
            "Value",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Content<Setter>(
            "Setters",
            ContentKind::Collection)
        .Factory();
    status = trigger.Finish();
    if (!status) return status.GetStatus();

    auto style = Describe<Style>(context);
    style
        .Property({
            "TargetType",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property({
            "BasedOn",
            Style::StaticTypeId(),
            PropertyFlags::None})
        .Property(OrdinaryProperty(
            "Resources",
            ResourceDictionary::StaticTypeId(),
            nullptr,
            &SetStyleResources,
            PropertyFlags::Structural))
        .Property({
            "Triggers",
            PropertyTrigger::StaticTypeId(),
            PropertyFlags::Structural |
                PropertyFlags::Collection})
        .Content<Setter>(
            "Setters",
            ContentKind::Collection)
        .Factory();
    status = style.Finish();
    if (!status) return status.GetStatus();

    auto eventArgs = DescribeStruct<EventArgs>(context);
    status = eventArgs.Finish();
    if (!status) return status.GetStatus();
    auto routedEventArgs = DescribeStruct<RoutedEventArgs>(context);
    status = routedEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto inputEventArgs = DescribeStruct<InputEventArgs>(context);
    status = inputEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto mouseEventArgs = DescribeStruct<MouseEventArgs>(context);
    status = mouseEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto mouseButtonEventArgs = DescribeStruct<MouseButtonEventArgs>(context);
    status = mouseButtonEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto mouseWheelEventArgs = DescribeStruct<MouseWheelEventArgs>(context);
    status = mouseWheelEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto keyEventArgs = DescribeStruct<KeyEventArgs>(context);
    status = keyEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto textCompositionEventArgs = DescribeStruct<TextCompositionEventArgs>(context);
    status = textCompositionEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto focusEventArgs = DescribeStruct<KeyboardFocusChangedEventArgs>(context);
    status = focusEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto canExecuteEventArgs = DescribeStruct<CanExecuteRoutedEventArgs>(context);
    status = canExecuteEventArgs.Finish();
    if (!status) return status.GetStatus();
    auto executedEventArgs = DescribeStruct<ExecutedRoutedEventArgs>(context);
    status = executedEventArgs.Finish();
    if (!status) return status.GetStatus();

    auto length = DescribeStruct<Length>(context);
    length
        .ValueSemantics({sizeof(Length), alignof(Length), nullptr, nullptr,
            &EqualLength, nullptr, true})
        .TextConverter(&ConvertLength, &context.ValueRegistrations());
    status = length.Finish();
    if (!status) return status.GetStatus();

    auto thickness = DescribeStruct<Thickness>(context);
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

    auto color = DescribeStruct<Color>(context);
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

    auto horizontal = DescribeEnum<HorizontalAlignment, std::uint32_t>(context);
    horizontal
        .EnumValue("Stretch", HorizontalAlignment::Stretch)
        .EnumValue("Left", HorizontalAlignment::Left)
        .EnumValue("Center", HorizontalAlignment::Center)
        .EnumValue("Right", HorizontalAlignment::Right);
    status = horizontal.Finish();
    if (!status) return status.GetStatus();

    auto vertical = DescribeEnum<VerticalAlignment, std::uint32_t>(context);
    vertical
        .EnumValue("Stretch", VerticalAlignment::Stretch)
        .EnumValue("Top", VerticalAlignment::Top)
        .EnumValue("Center", VerticalAlignment::Center)
        .EnumValue("Bottom", VerticalAlignment::Bottom);
    status = vertical.Finish();
    if (!status) return status.GetStatus();

    auto command = Describe<ICommand>(context, TypeFlags::Abstract);
    status = command.Finish();
    if (!status) return status.GetStatus();
    auto inputGesture = Describe<InputGesture>(context, TypeFlags::Abstract);
    status = inputGesture.Finish();
    if (!status) return status.GetStatus();
    auto keyGesture = Describe<KeyGesture>(context);
    status = keyGesture.Finish();
    if (!status) return status.GetStatus();
    auto routedCommand = Describe<RoutedCommand>(context);
    status = routedCommand.Finish();
    if (!status) return status.GetStatus();

    auto visual = Describe<Visual>(context, TypeFlags::Abstract);
    status = visual.Finish();
    if (!status) return status.GetStatus();

    auto uiElement = Describe<UIElement>(context, TypeFlags::Abstract);
    if (context.RoutedEvents() != nullptr) {
        uiElement
            .Event(
                UIElement::MouseMoveEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::MouseDownEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::MouseUpEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::MouseWheelEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::GotKeyboardFocusEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::LostKeyboardFocusEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::KeyDownEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::KeyUpEvent,
                RoutingStrategy::Bubble)
            .Event(
                UIElement::TextInputEvent,
                RoutingStrategy::Bubble);
    }
    uiElement
        .Property(
            UIElement::ClipToBoundsProperty,
            false,
            PropertyMetadataFlags::AffectsArrange)
        .Property(
            UIElement::IsHitTestVisibleProperty,
            true,
            PropertyMetadataFlags::None)
        .Property(
            UIElement::IsEnabledProperty,
            true,
            PropertyMetadataFlags::Inherits |
                PropertyMetadataFlags::AffectsRender)
        .ReadOnlyProperty(
            UIElement::IsMouseOverProperty,
            false,
            PropertyMetadataFlags::AffectsRender)
        .ReadOnlyProperty(
            UIElement::IsPressedProperty,
            false,
            PropertyMetadataFlags::AffectsRender)
        .ReadOnlyProperty(
            UIElement::IsKeyboardFocusedProperty,
            false,
            PropertyMetadataFlags::AffectsRender)
        .Property(
            UIElement::IsTabStopProperty,
            false,
            PropertyMetadataFlags::None)
        .Property(
            UIElement::TabIndexProperty,
            0U,
            PropertyMetadataFlags::None,
            &ValidateUInt32)
        .Property(
            UIElement::IsFocusScopeProperty,
            false,
            PropertyMetadataFlags::None);
    status = uiElement.Finish();
    if (!status) return status.GetStatus();

    auto frameworkElement = Describe<FrameworkElement>(context, TypeFlags::Abstract);
    const Length autoSource = Length::Auto();
    Base::Result<Value> automatic = context.Values().TryCreateValue(
        TypeOf<Length>(), &autoSource);
    if (!automatic) return automatic.GetStatus();
    const Thickness zero{};
    Base::Result<Value> margin = context.Values().TryCreateValue(
        TypeOf<Thickness>(), &zero);
    if (!margin) return margin.GetStatus();
    frameworkElement
        .Property(OrdinaryProperty(
            "Resources",
            ResourceDictionary::StaticTypeId(),
            nullptr,
            &SetFrameworkElementResources,
            PropertyFlags::Structural))
        .Property(
            FrameworkElement::DataContextProperty,
            Value::NullObject(TypeOf<Base::Object>()),
            PropertyMetadataFlags::Inherits)
        .Property(
            FrameworkElement::StyleProperty,
            Value::NullObject(Style::StaticTypeId()),
            PropertyMetadataFlags::None)
        .Property(
            FrameworkElement::WidthProperty,
            automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateLength)
        .Property(
            FrameworkElement::HeightProperty,
            automatic.Value(),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateLength)
        .Property(
            FrameworkElement::MinWidthProperty,
            0.0,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble,
            &CoerceMinWidth)
        .Property(
            FrameworkElement::MaxWidthProperty,
            DefaultMaximum,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble,
            &CoerceMaxWidth)
        .Property(
            FrameworkElement::MinHeightProperty,
            0.0,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble,
            &CoerceMinHeight)
        .Property(
            FrameworkElement::MaxHeightProperty,
            DefaultMaximum,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateNonnegativeDouble,
            &CoerceMaxHeight)
        .Property(
            FrameworkElement::MarginProperty,
            margin.Value(),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateThicknessValue)
        .Property(
            FrameworkElement::HorizontalAlignmentProperty,
            Value::FromUnsignedInteger(
                TypeOf<HorizontalAlignment>(), 0U),
            PropertyMetadataFlags::AffectsArrange)
        .Property(
            FrameworkElement::VerticalAlignmentProperty,
            Value::FromUnsignedInteger(
                TypeOf<VerticalAlignment>(), 0U),
            PropertyMetadataFlags::AffectsArrange)
        .Property(
            FrameworkElement::UseLayoutRoundingProperty,
            false,
            PropertyMetadataFlags::AffectsMeasure);
    status = frameworkElement.Finish();
    return status;
}

} // namespace Aero::Presentation

#include <Aero/Presentation/Metadata.hpp>

#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

constexpr double DefaultMaximum = 1.0e12;

Base::Result<Length> ConvertLength(
    Base::StringView text) noexcept {
    const Base::StringView value = ValueConversion::Trim(text);
    Length length = Length::Auto();
    if (!ValueConversion::EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed =
            ValueConversion::ParseDouble(value);
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

Base::Result<Thickness> ConvertThickness(
    Base::StringView text) noexcept {
    return ParseThickness(text);
}

int Hex(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Color> ConvertColor(
    Base::StringView text) noexcept {
    const Base::StringView value = ValueConversion::Trim(text);
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

bool ValidateLength(const Length& length) noexcept {
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateThicknessValue(const Thickness& t) noexcept {
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
}
template<class TProperty>
Base::Result<double> CheckMinimum(
    DependencyObject& object,
    const double& value,
    const TProperty& maximum) noexcept {
    Base::Result<double> other = object.GetValue(maximum);
    if (!other || value > other.Value()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Minimum layout size exceeds maximum layout size");
    }
    return value;
}
template<class TProperty>
Base::Result<double> CheckMaximum(
    DependencyObject& object,
    const double& value,
    const TProperty& minimum) noexcept {
    Base::Result<double> other = object.GetValue(minimum);
    if (!other || value < other.Value()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Maximum layout size is below minimum layout size");
    }
    return value;
}
Base::Result<double> CoerceMinWidth(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMinimum(
        object, value, FrameworkElement::MaxWidthProperty);
}
Base::Result<double> CoerceMaxWidth(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMaximum(
        object, value, FrameworkElement::MinWidthProperty);
}
Base::Result<double> CoerceMinHeight(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMinimum(
        object, value, FrameworkElement::MaxHeightProperty);
}
Base::Result<double> CoerceMaxHeight(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMaximum(
        object, value, FrameworkElement::MinHeightProperty);
}

TypeReference GetStyleTargetType(
    const Style& style) noexcept {
    return {style.TargetType()};
}

Base::Result<void> SetStyleTargetType(
    Style& style,
    TypeReference value) noexcept {
    return style.TrySetTargetType(value.type);
}

Base::Result<void> SetStyleBasedOn(
    Style& style,
    Base::Ref<Style> value) noexcept {
    return style.TrySetBasedOn(
        Base::Ref<Base::Object>(
            std::move(value)));
}

Base::Result<void> AddMergedDictionary(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<ResourceDictionary&>(owner)
        .TryAddMerged(
            static_cast<ResourceDictionary&>(*value));
}

Base::Result<void> ClearMergedDictionaries(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ResourceDictionary&>(owner)
        .ClearMergedDictionaries();
}

Base::Result<void> AddStyleSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Setter cannot be retained");
    }
    return static_cast<Style&>(owner)
        .TryAddAuthoredSetter(
            std::move(retained));
}

Base::Result<void> ClearStyleSetters(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Style&>(owner)
        .ClearAuthoredSetters();
}

Base::Result<void> AddStyleTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<PropertyTrigger> retained =
        Base::Ref<PropertyTrigger>::TryFromBorrowed(
            static_cast<PropertyTrigger&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Trigger cannot be retained");
    }
    return static_cast<Style&>(owner)
        .TryAddAuthoredTrigger(
            std::move(retained));
}

Base::Result<void> ClearStyleTriggers(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Style&>(owner)
        .ClearAuthoredTriggers();
}

Base::Result<void> AddTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Trigger Setter cannot be retained");
    }
    return static_cast<PropertyTrigger&>(owner)
        .TryAddAuthoredSetter(
            std::move(retained));
}

Base::Result<void> ClearTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<PropertyTrigger&>(owner)
        .ClearAuthoredSetters();
}

} // namespace

Base::Result<void> Detail::PopulatePresentationMetadata(
    Core::MetadataContext& context) noexcept {
    Base::Result<void> status;

    auto resourceDictionary =
        Describe<ResourceDictionary>(context);
    resourceDictionary
        .Property<
            Base::ResourceUri,
            &ResourceDictionary::Source,
            &ResourceDictionary::SetSource>(
            "Source",
            PropertyFlags::None)
        .Collection<ResourceDictionary>(
            "MergedDictionaries",
            &AddMergedDictionary,
            &ClearMergedDictionaries)
        .Content<Base::Object>(
            "Entries",
            ContentKind::Collection)
        .Factory();
    status = resourceDictionary.Result();
    if (!status) return status.GetStatus();

    auto setter = Describe<Setter>(context);
    setter
        .Property(
            "TargetName",
            &Setter::TargetName,
            &Setter::SetTargetName)
        .Property(
            "Property",
            &Setter::PropertyName,
            &Setter::SetPropertyName)
        .Property<
            Value,
            &Setter::AuthoredValue,
            &Setter::SetAuthoredValue>(
            "Value",
            PropertyFlags::AnyValue)
        .Factory();
    status = setter.Result();
    if (!status) return status.GetStatus();

    auto trigger = Describe<PropertyTrigger>(context);
    trigger
        .Property(
            "Property",
            &PropertyTrigger::PropertyName,
            &PropertyTrigger::SetPropertyName)
        .Property<
            Value,
            &PropertyTrigger::AuthoredValue,
            &PropertyTrigger::SetAuthoredValue>(
            "Value",
            PropertyFlags::AnyValue)
        .Content<Setter>(
            "Setters",
            ContentKind::Collection,
            &AddTriggerSetter,
            &ClearTriggerSetters)
        .Factory();
    status = trigger.Result();
    if (!status) return status.GetStatus();

    auto style = Describe<Style>(context);
    style
        .Property<
            TypeReference,
            &GetStyleTargetType,
            &SetStyleTargetType>(
            "TargetType",
            PropertyFlags::None)
        .Property<
            Base::Ref<Style>,
            &SetStyleBasedOn>(
            "BasedOn",
            PropertyFlags::WriteOnly)
        .Property<
            Base::Ref<ResourceDictionary>,
            &Style::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Collection<PropertyTrigger>(
            "Triggers",
            &AddStyleTrigger,
            &ClearStyleTriggers)
        .Content<Setter>(
            "Setters",
            ContentKind::Collection,
            &AddStyleSetter,
            &ClearStyleSetters)
        .Factory();
    status = style.Result();
    if (!status) return status.GetStatus();

    status = Describe<EventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<RoutedEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<InputEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<MouseEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<MouseButtonEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<MouseWheelEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<KeyEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<TextCompositionEventArgs>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<KeyboardFocusChangedEventArgs>(
        context).Result();
    if (!status) return status.GetStatus();
    status = Describe<CanExecuteRoutedEventArgs>(
        context).Result();
    if (!status) return status.GetStatus();
    status = Describe<ExecutedRoutedEventArgs>(
        context).Result();
    if (!status) return status.GetStatus();

    auto length = Describe<Length>(context);
    length
        .ValueSemantics({sizeof(Length), alignof(Length), nullptr, nullptr,
            &EqualLength, nullptr, true})
        .TextConverter<&ConvertLength>();
    status = length.Result();
    if (!status) return status.GetStatus();

    auto thickness = Describe<Thickness>(context);
    thickness
        .Field<&Thickness::left>("Left")
        .Field<&Thickness::top>("Top")
        .Field<&Thickness::right>("Right")
        .Field<&Thickness::bottom>("Bottom")
        .ValueSemantics({sizeof(Thickness), alignof(Thickness), nullptr,
            nullptr, &EqualThickness, nullptr, true})
        .TextConverter<&ConvertThickness>();
    status = thickness.Result();
    if (!status) return status.GetStatus();

    auto color = Describe<Color>(context);
    color
        .Field<&Color::red>("Red")
        .Field<&Color::green>("Green")
        .Field<&Color::blue>("Blue")
        .Field<&Color::alpha>("Alpha")
        .ValueSemantics({sizeof(Color), alignof(Color), nullptr, nullptr,
            &EqualColor, nullptr, true})
        .TextConverter<&ConvertColor>();
    status = color.Result();
    if (!status) return status.GetStatus();

    auto horizontal = Describe<HorizontalAlignment>(context);
    horizontal
        .Value("Stretch", HorizontalAlignment::Stretch)
        .Value("Left", HorizontalAlignment::Left)
        .Value("Center", HorizontalAlignment::Center)
        .Value("Right", HorizontalAlignment::Right);
    status = horizontal.Result();
    if (!status) return status.GetStatus();

    auto vertical = Describe<VerticalAlignment>(context);
    vertical
        .Value("Stretch", VerticalAlignment::Stretch)
        .Value("Top", VerticalAlignment::Top)
        .Value("Center", VerticalAlignment::Center)
        .Value("Bottom", VerticalAlignment::Bottom);
    status = vertical.Result();
    if (!status) return status.GetStatus();

    status = Describe<ICommand>(
        context, TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();
    status = Describe<InputGesture>(
        context, TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();
    status = Describe<KeyGesture>(context).Result();
    if (!status) return status.GetStatus();
    status = Describe<RoutedCommand>(context).Result();
    if (!status) return status.GetStatus();

    status = Describe<Visual>(
        context, TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();

    auto uiElement = Describe<UIElement>(
        context, TypeFlags::Abstract);
    uiElement
        .Event(UIElement::MouseMoveEvent)
        .Event(UIElement::MouseDownEvent)
        .Event(UIElement::MouseUpEvent)
        .Event(UIElement::MouseWheelEvent)
        .Event(UIElement::GotKeyboardFocusEvent)
        .Event(UIElement::LostKeyboardFocusEvent)
        .Event(UIElement::KeyDownEvent)
        .Event(UIElement::KeyUpEvent)
        .Event(UIElement::TextInputEvent)
        .Property(
            UIElement::ClipToBoundsProperty,
            PropertyOptions(false).AffectsArrange())
        .Property(
            UIElement::IsHitTestVisibleProperty,
            PropertyOptions(true))
        .Property(
            UIElement::IsEnabledProperty,
            PropertyOptions(true)
                .Inherits()
                .AffectsRender())
        .Property(
            UIElement::IsMouseOverProperty,
            PropertyOptions(false).AffectsRender())
        .Property(
            UIElement::IsPressedProperty,
            PropertyOptions(false).AffectsRender())
        .Property(
            UIElement::IsKeyboardFocusedProperty,
            PropertyOptions(false).AffectsRender())
        .Property(
            UIElement::IsTabStopProperty,
            PropertyOptions(false))
        .Property(
            UIElement::TabIndexProperty,
            PropertyOptions(std::uint32_t{0}))
        .Property(
            UIElement::IsFocusScopeProperty,
            PropertyOptions(false));
    status = uiElement.Result();
    if (!status) return status.GetStatus();

    auto frameworkElement = Describe<FrameworkElement>(
        context, TypeFlags::Abstract);
    frameworkElement
        .Property<
            Base::Ref<ResourceDictionary>,
            &FrameworkElement::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Property(
            FrameworkElement::DataContextProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .Inherits())
        .Property(
            FrameworkElement::StyleProperty,
            PropertyOptions(Base::Ref<Style>{}))
        .Property(
            FrameworkElement::WidthProperty,
            PropertyOptions(Length::Auto())
                .AffectsMeasure()
                .Validate(&ValidateLength))
        .Property(
            FrameworkElement::HeightProperty,
            PropertyOptions(Length::Auto())
                .AffectsMeasure()
                .Validate(&ValidateLength))
        .Property(
            FrameworkElement::MinWidthProperty,
            PropertyOptions(0.0)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>)
                .Coerce(&CoerceMinWidth))
        .Property(
            FrameworkElement::MaxWidthProperty,
            PropertyOptions(DefaultMaximum)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>)
                .Coerce(&CoerceMaxWidth))
        .Property(
            FrameworkElement::MinHeightProperty,
            PropertyOptions(0.0)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>)
                .Coerce(&CoerceMinHeight))
        .Property(
            FrameworkElement::MaxHeightProperty,
            PropertyOptions(DefaultMaximum)
                .AffectsMeasure()
                .Validate(&Validate::NonNegative<double>)
                .Coerce(&CoerceMaxHeight))
        .Property(
            FrameworkElement::MarginProperty,
            PropertyOptions(Thickness{})
                .AffectsMeasure()
                .Validate(&ValidateThicknessValue))
        .Property(
            FrameworkElement::HorizontalAlignmentProperty,
            PropertyOptions(HorizontalAlignment::Stretch)
                .AffectsArrange())
        .Property(
            FrameworkElement::VerticalAlignmentProperty,
            PropertyOptions(VerticalAlignment::Stretch)
                .AffectsArrange())
        .Property(
            FrameworkElement::UseLayoutRoundingProperty,
            PropertyOptions(false).AffectsMeasure());
    status = frameworkElement.Result();
    return status;
}

} // namespace Aero::Presentation

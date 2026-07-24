#include <Aero/Controls/Metadata.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/MetadataDsl.hpp>

#include <cctype>
#include <cmath>
#include <limits>

namespace Aero::Controls {
namespace {

bool EqualsAsciiInsensitive(
    Base::StringView value,
    const char* literal) noexcept {
    std::uint32_t size = 0U;
    while (literal[size] != '\0') ++size;
    if (value.SizeBytes() != size) return false;
    for (std::uint32_t index = 0U; index < size; ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(literal[index]))) {
            return false;
        }
    }
    return true;
}

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

Base::Result<Core::Value> ConvertOrientation(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "horizontal")) {
        return Core::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(Orientation::Horizontal));
    }
    if (EqualsAsciiInsensitive(value, "vertical")) {
        return Core::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(Orientation::Vertical));
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Orientation is invalid");
}

Base::Result<Core::Value> ConvertClickMode(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "release")) {
        return Core::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(ClickMode::Release));
    }
    if (EqualsAsciiInsensitive(value, "press")) {
        return Core::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(ClickMode::Press));
    }
    if (EqualsAsciiInsensitive(value, "hover")) {
        return Core::Value::FromUnsignedInteger(
            type, static_cast<std::uint64_t>(ClickMode::Hover));
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "ClickMode is invalid");
}

bool ValidateNonnegativeDouble(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Double &&
        std::isfinite(value.AsDouble()) && value.AsDouble() >= 0.0;
}

bool ValidateFiniteDouble(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Double &&
        std::isfinite(value.AsDouble());
}

bool ValidateThicknessValue(const Core::Value& value) noexcept {
    if (value.Kind() != Core::ValueKind::Custom) return false;
    const Presentation::Thickness& thickness =
        *static_cast<const Presentation::Thickness*>(value.AsCustom());
    return Presentation::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

bool ValidateColorValue(const Core::Value& value) noexcept {
    if (value.Kind() != Core::ValueKind::Custom) return false;
    const Presentation::Color& color =
        *static_cast<const Presentation::Color*>(value.AsCustom());
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}

bool ValidateOrientationValue(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            static_cast<std::uint64_t>(Orientation::Vertical);
}

bool ValidateUInt32(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            std::numeric_limits<std::uint32_t>::max();
}

bool ValidateClickModeValue(
    const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            static_cast<std::uint64_t>(ClickMode::Hover);
}

Base::Result<Core::Value> CoerceButtonEnabled(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Core::Value& value) noexcept {
    const bool enabled = value.AsBoolean() &&
        static_cast<ButtonBase&>(object).IsCommandEnabled();
    return Core::Value::FromBoolean(
        TypeOf<bool>(), enabled);
}

} // namespace

Base::Result<void> Detail::PopulateControlsMetadata(
    Core::MetaRegistrationContext& context) noexcept {
    using namespace Aero::Core;
    Base::Result<void> status;

    MetaTypeBuilder<Orientation> orientation =
        MetaTypeBuilder<Orientation>::Enum(
            context, TypeOf<std::uint32_t>());
    orientation
        .EnumValue("Horizontal", Orientation::Horizontal)
        .EnumValue("Vertical", Orientation::Vertical)
        .TextConverter(&ConvertOrientation);
    status = orientation.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ClickMode> clickMode =
        MetaTypeBuilder<ClickMode>::Enum(
            context, TypeOf<std::uint32_t>());
    clickMode
        .EnumValue("Release", ClickMode::Release)
        .EnumValue("Press", ClickMode::Press)
        .EnumValue("Hover", ClickMode::Hover)
        .TextConverter(&ConvertClickMode);
    status = clickMode.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Panel> panel =
        MetaTypeBuilder<Panel>::Object(context, TypeFlags::Abstract);
    panel.Content<Presentation::UIElement>(
        "Children", ContentKind::Collection);
    status = panel.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Decorator> decorator =
        MetaTypeBuilder<Decorator>::Object(context, TypeFlags::Abstract);
    decorator.Content<Presentation::UIElement>(
        "Content", ContentKind::Single);
    status = decorator.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Control> control =
        MetaTypeBuilder<Control>::Object(context, TypeFlags::Abstract);
    status = control.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ContentControl> contentControl =
        MetaTypeBuilder<ContentControl>::Object(
            context, TypeFlags::Abstract);
    contentControl.Content<Presentation::UIElement>(
        "Content", ContentKind::Single);
    status = contentControl.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ButtonBase> buttonBase =
        MetaTypeBuilder<ButtonBase>::Object(
            context, TypeFlags::Abstract);
    if (context.RoutedEvents() != nullptr) {
        buttonBase.RoutedEvent(
            ButtonBase::ClickEvent,
            "Click", TypeOf<RoutedEventArgs>(),
            RoutingStrategy::Bubble);
    }
    buttonBase
        .DependencyProperty(
            ButtonBase::ClickModeProperty,
            "ClickMode", TypeOf<ClickMode>(),
            Value::FromUnsignedInteger(
                TypeOf<ClickMode>(),
                static_cast<std::uint64_t>(
                    ClickMode::Release)),
            PropertyMetadataFlags::None,
            &ValidateClickModeValue)
        .DependencyProperty(
            ButtonBase::CommandProperty,
            "Command", TypeOf<ICommand>(),
            Value::NullObject(TypeOf<ICommand>()),
            PropertyMetadataFlags::None)
        .DependencyProperty(
            ButtonBase::CommandParameterProperty,
            "CommandParameter", TypeOf<Base::Object>(),
            Value::NullObject(TypeOf<Base::Object>()),
            PropertyMetadataFlags::None)
        .DependencyProperty(
            ButtonBase::CommandTargetProperty,
            "CommandTarget", TypeOf<UIElement>(),
            Value::NullObject(TypeOf<UIElement>()),
            PropertyMetadataFlags::None)
        .Content<Presentation::UIElement>(
            "Content", ContentKind::Single);
    status = buttonBase.Finish();
    if (!status) return status.GetStatus();
    PropertyMetadata buttonEnabled;
    buttonEnabled.defaultValue =
        Value::FromBoolean(TypeOf<bool>(), true);
    buttonEnabled.flags =
        PropertyMetadataFlags::Inherits |
        PropertyMetadataFlags::AffectsRender;
    buttonEnabled.coerce = &CoerceButtonEnabled;
    status = context.DependencyProperties().TryOverrideMetadata(
        UIElement::IsEnabledProperty,
        TypeOf<ButtonBase>(), buttonEnabled);
    if (!status) return status.GetStatus();
    PropertyMetadata buttonTabStop;
    buttonTabStop.defaultValue =
        Value::FromBoolean(TypeOf<bool>(), true);
    status = context.DependencyProperties().TryOverrideMetadata(
        UIElement::IsTabStopProperty,
        TypeOf<ButtonBase>(), buttonTabStop);
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Button> button =
        MetaTypeBuilder<Button>::Object(context);
    button.Content<Presentation::UIElement>(
        "Content", ContentKind::Single);
    status = button.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<UserControl> userControl =
        MetaTypeBuilder<UserControl>::Object(context);
    status = userControl.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<StackPanel> stackPanel =
        MetaTypeBuilder<StackPanel>::Object(context);
    stackPanel
        .DependencyProperty(StackPanel::OrientationProperty, "Orientation",
            TypeOf<Orientation>(),
            Value::FromUnsignedInteger(TypeOf<Orientation>(), 1U),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateOrientationValue)
        .Content<Presentation::UIElement>(
            "Children", ContentKind::Collection);
    status = stackPanel.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Canvas> canvas =
        MetaTypeBuilder<Canvas>::Object(context);
    canvas
        .AttachedDependencyProperty(Canvas::LeftProperty, "Left",
            TypeOf<double>(), Value::FromDouble(TypeOf<double>(), 0.0),
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .AttachedDependencyProperty(Canvas::TopProperty, "Top",
            TypeOf<double>(), Value::FromDouble(TypeOf<double>(), 0.0),
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .Content<Presentation::UIElement>(
            "Children", ContentKind::Collection);
    status = canvas.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<Grid> grid =
        MetaTypeBuilder<Grid>::Object(context);
    grid
        .AttachedDependencyProperty(Grid::RowProperty, "Row",
            TypeOf<std::uint32_t>(),
            Value::FromUnsignedInteger(TypeOf<std::uint32_t>(), 0U),
            PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32)
        .AttachedDependencyProperty(Grid::ColumnProperty, "Column",
            TypeOf<std::uint32_t>(),
            Value::FromUnsignedInteger(TypeOf<std::uint32_t>(), 0U),
            PropertyMetadataFlags::AffectsParentMeasure, &ValidateUInt32)
        .Content<Presentation::UIElement>(
            "Children", ContentKind::Collection);
    status = grid.Finish();
    if (!status) return status.GetStatus();

    const Presentation::Color transparent{};
    Base::Result<Value> transparentValue =
        context.Values().TryCreateValue(
            TypeOf<Presentation::Color>(), &transparent);
    if (!transparentValue) return transparentValue.GetStatus();
    const Presentation::Thickness zero{};
    Base::Result<Value> padding =
        context.Values().TryCreateValue(
            TypeOf<Presentation::Thickness>(), &zero);
    if (!padding) return padding.GetStatus();

    MetaTypeBuilder<Border> border =
        MetaTypeBuilder<Border>::Object(context);
    border
        .DependencyProperty(Border::BackgroundProperty, "Background",
            TypeOf<Presentation::Color>(), transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue)
        .DependencyProperty(Border::BorderBrushProperty, "BorderBrush",
            TypeOf<Presentation::Color>(), transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue)
        .DependencyProperty(Border::BorderThicknessProperty,
            "BorderThickness", TypeOf<double>(),
            Value::FromDouble(TypeOf<double>(), 0.0),
            PropertyMetadataFlags::AffectsRender,
            &ValidateNonnegativeDouble)
        .DependencyProperty(Border::PaddingProperty, "Padding",
            TypeOf<Presentation::Thickness>(), padding.Value(),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateThicknessValue)
        .Content<Presentation::UIElement>(
            "Content", ContentKind::Single);
    status = border.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<TextBlock> textBlock =
        MetaTypeBuilder<TextBlock>::Object(context);
    Base::Result<Value> text =
        Value::TryFromString(TypeOf<Base::String>(), {});
    if (!text) return text.GetStatus();
    const Presentation::Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> foreground =
        context.Values().TryCreateValue(
            TypeOf<Presentation::Color>(), &black);
    if (!foreground) return foreground.GetStatus();
    textBlock
        .DependencyProperty(TextBlock::TextProperty, "Text",
            TypeOf<Base::String>(), text.Value(),
            PropertyMetadataFlags::AffectsMeasure)
        .DependencyProperty(TextBlock::ForegroundProperty, "Foreground",
            TypeOf<Presentation::Color>(), foreground.Value(),
            PropertyMetadataFlags::AffectsRender, &ValidateColorValue);
    status = textBlock.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ContentPresenter> contentPresenter =
        MetaTypeBuilder<ContentPresenter>::Object(context);
    contentPresenter.Content<Presentation::UIElement>(
        "Content", ContentKind::Single);
    return contentPresenter.Finish();
}

} // namespace Aero::Controls

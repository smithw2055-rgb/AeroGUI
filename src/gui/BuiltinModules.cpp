// Consolidated built-in UI / markup module registration.
// One TU; sections are module registration methods, not separate .inl files.

#include "BuiltinModules.hpp"

#include "gui/core/Describe.hpp"
#include "gui/core/EnumRegistration.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/AnimationModel.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/markup/XamlObjectWriterState.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Animatable.hpp>
#include <Aero/DispatcherObject.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/Data/SortDescription.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/VirtualizingWrapPanel.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Ranges.hpp>
#include <Aero/Controls/GridSplitter.hpp>
#include <Aero/Controls/ScrollViewer.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/UserControl.hpp>
#include <Aero/Controls/Headers.hpp>
#include <Aero/Controls/Label.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Selectors.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Separator.hpp>
#include <Aero/Controls/ToolBar.hpp>
#include <Aero/Controls/StatusBar.hpp>
#include <Aero/Controls/ToolTip.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Controls/GridViews.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Input.hpp>
#include <Aero/VisualStateManager.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

// ===== RegisterCoreMetadata / PopulateCoreMetadata =====

namespace Aero::Meta {
Base::Result<void> PopulateCoreMetadata(
    Meta::Registration& context) noexcept {

    Register<Base::Object>(context);

    Register<bool>(context)
        .TextConverter<&Base::ValueConversion::ConvertBoolean>();

    Register<::Aero::Nullable<bool>>(context)
        .TextConverter<&Base::ValueConversion::ConvertNullableBoolean>();

    Register<std::int8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int8_t>>();
    Register<std::int16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int16_t>>();
    Register<std::int32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int32_t>>();
    Register<std::int64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int64_t>>();
    Register<std::uint8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint8_t>>();
    Register<std::uint16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint16_t>>();
    Register<std::uint32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint32_t>>();
    Register<std::uint64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint64_t>>();

    Register<double>(context)
        .TextConverter<&Base::ValueConversion::ConvertDouble>();

    Register<Base::String>(context)
        .TextConverter<&Base::ValueConversion::ConvertString>();

    Register<Value>(context)
        .ValueSemantics();

    Register<TypeReference>(context);

    Register<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&Base::ValueConversion::ConvertResourceUri>();

    Register<Threading::DispatcherObject>(context, TypeFlags::Abstract);

    Register<DependencyObject>(context, TypeFlags::Abstract);

    return Meta::Register<Freezable>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Meta

// ===== PopulateEnumMetadata =====

namespace Aero {

Base::Result<void> PopulateEnumMetadata(
    Meta::Registration& context) noexcept {
    // 谁定义谁注册: family TUs own AERO_REGISTER_ENUM bodies; this only
    // orchestrates call order. App enums live in src/app/Metadata.cpp.
    Base::Result<void> status;
    status = PopulateInputEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateAnimationEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateInteractivityEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateElementEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateMediaEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateShapesEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateDataEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateTextEnums(context);
    if (!status) return status.GetStatus();
    status = PopulateControlsEnums(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero

// ===== PopulateUiInput / PopulateInputDevices =====

namespace Aero {

Base::Result<void> PopulateUiInput(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::EventArgs>::Run(context);
    DescribeHook<::Aero::RoutedEventArgs>::Run(context);
    DescribeHook<::Aero::InputEventArgs>::Run(context);
    DescribeHook<::Aero::MouseEventArgs>::Run(context);
    DescribeHook<::Aero::MouseButtonEventArgs>::Run(context);
    DescribeHook<::Aero::MouseWheelEventArgs>::Run(context);
    DescribeHook<::Aero::DragEventArgs>::Run(context);
    DescribeHook<::Aero::GiveFeedbackEventArgs>::Run(context);
    DescribeHook<::Aero::DragCompletedEventArgs>::Run(context);
    DescribeHook<::Aero::KeyEventArgs>::Run(context);
    DescribeHook<::Aero::TextCompositionEventArgs>::Run(context);
    DescribeHook<::Aero::KeyboardFocusChangedEventArgs>::Run(context);
    DescribeHook<::Aero::Input::KeyboardNavigation>::Run(context);
    DescribeHook<::Aero::Input::FocusManager>::Run(context);
    DescribeHook<::Aero::CanExecuteRoutedEventArgs>::Run(context);
    DescribeHook<::Aero::ExecutedRoutedEventArgs>::Run(context);
    DescribeHook<::Aero::Input::Cursor>::Run(context);
    return {};
}

} // namespace Aero

namespace Aero {

Base::Result<void> PopulateInputDevices(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::Input::Mouse>::Run(context);
    DescribeHook<::Aero::Input::Keyboard>::Run(context);
    DescribeHook<::Aero::DataObject>::Run(context);
    DescribeHook<::Aero::DragDrop>::Run(context);
    return {};
}

} // namespace Aero

// ===== Media value converters (MetadataSupport) =====

namespace Aero::MetadataSupport {
using namespace ::Aero::Media;
namespace {


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

// ===== PopulateUiMedia =====

namespace Aero {
using namespace ::Aero::MetadataSupport;
using namespace ::Aero::Meta;
using namespace ::Aero::Input;

Base::Result<void> PopulateUiMedia(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status;
    DescribeHook<::Aero::Length>::Run(context);
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
    DescribeHook<::Aero::Animatable>::Run(context);

    // Brush.RelativeTransform is a Transform-valued dependency property, so
    // the abstract value type must exist before Brush metadata is authored.
    DescribeHook<::Aero::Media::Transform>::Run(context);
    DescribeHook<::Aero::Media::Brush>::Run(context);
    DescribeHook<::Aero::Media::SolidColorBrush>::Run(context);
    DescribeHook<::Aero::Media::GradientStop>::Run(context);
    DescribeHook<::Aero::Media::GradientStopCollection>::Run(context);
    DescribeHook<::Aero::Media::BrushShader>::Run(context);
    DescribeHook<::Aero::Media::MonochromeShader>::Run(context);
    DescribeHook<::Aero::Media::ConicGradientShader>::Run(context);
    DescribeHook<::Aero::Media::WavesShader>::Run(context);
    DescribeHook<::Aero::Media::GradientBrush>::Run(context);
    DescribeHook<::Aero::Media::LinearGradientBrush>::Run(context);
    DescribeHook<::Aero::Media::RadialGradientBrush>::Run(context);
    DescribeHook<::Aero::Media::ImageSource>::Run(context);
    DescribeHook<::Aero::Media::TileBrush>::Run(context);
    DescribeHook<::Aero::Media::BitmapImage>::Run(context);
    DescribeHook<::Aero::Media::CroppedBitmap>::Run(context);
    DescribeHook<::Aero::Media::ImageBrush>::Run(context);
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
    DescribeHook<::Aero::Media::TranslateTransform>::Run(context);
    DescribeHook<::Aero::Media::ScaleTransform>::Run(context);
    DescribeHook<::Aero::Media::RotateTransform>::Run(context);
    DescribeHook<::Aero::Media::SkewTransform>::Run(context);
    DescribeHook<::Aero::Media::MatrixTransform>::Run(context);
    DescribeHook<::Aero::Media::CompositeTransform>::Run(context);
    DescribeHook<::Aero::Media::TransformGroup>::Run(context);
    DescribeHook<::Aero::Media::Transform3D>::Run(context);
    DescribeHook<::Aero::Media::CompositeTransform3D>::Run(context);
    DescribeHook<::Aero::Media::PerspectiveTransform3D>::Run(context);
    DescribeHook<::Aero::Media::MatrixTransform3D>::Run(context);
    DescribeHook<::Aero::Media::Effect>::Run(context);
    DescribeHook<::Aero::Media::BlurEffect>::Run(context);
    DescribeHook<::Aero::Media::DropShadowEffect>::Run(context);
    DescribeHook<::Aero::Media::PixelateEffect>::Run(context);
    DescribeHook<::Aero::Media::TintEffect>::Run(context);
    DescribeHook<::Aero::Media::DirectionalBlurEffect>::Run(context);
    DescribeHook<::Aero::Media::ShaderEffect>::Run(context);
    DescribeHook<::Aero::Media::MediaElement>::Run(context);
    DescribeHook<::Aero::Media::VisualBrush>::Run(context);
    DescribeHook<::Aero::Input::ICommand>::Run(context);
    DescribeHook<::Aero::Input::InputGesture>::Run(context);
    DescribeHook<::Aero::Input::KeyGesture>::Run(context);
    DescribeHook<::Aero::Input::RoutedCommand>::Run(context);
    DescribeHook<::Aero::Input::RoutedUICommand>::Run(context);
    DescribeHook<::Aero::Input::InputBinding>::Run(context);
    DescribeHook<::Aero::Input::CommandBinding>::Run(context);
    DescribeHook<::Aero::Input::ApplicationCommands>::Run(context);
    status = ApplicationCommands::RegisterDefaults();
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero

// ===== PopulateUiResources =====

namespace Aero {

Base::Result<void> PopulateUiResources(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::ResourceDictionary>::Run(context);
    DescribeHook<::Aero::Media::Geometry>::Run(context);
    DescribeHook<::Aero::Media::DashStyle>::Run(context);
    DescribeHook<::Aero::Media::Pen>::Run(context);
    DescribeHook<::Aero::Media::StreamGeometry>::Run(context);
    DescribeHook<::Aero::Media::PathSegment>::Run(context);
    DescribeHook<::Aero::Media::LineSegment>::Run(context);
    DescribeHook<::Aero::Media::PathFigure>::Run(context);
    DescribeHook<::Aero::Media::PathGeometry>::Run(context);
    DescribeHook<::Aero::Media::BezierSegment>::Run(context);
    DescribeHook<::Aero::Media::QuadraticBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::ArcSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyLineSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::PolyQuadraticBezierSegment>::Run(context);
    DescribeHook<::Aero::Media::LineGeometry>::Run(context);
    DescribeHook<::Aero::Media::RectangleGeometry>::Run(context);
    DescribeHook<::Aero::Media::EllipseGeometry>::Run(context);
    DescribeHook<::Aero::Media::GeometryGroup>::Run(context);
    DescribeHook<::Aero::Media::CombinedGeometry>::Run(context);
    DescribeHook<::Aero::Media::FontFamily>::Run(context);
    return {};
}

} // namespace Aero

// ===== PopulateUiStyling =====

namespace Aero {

Base::Result<void> PopulateUiStyling(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::Element>::Run(context);
    DescribeHook<::Aero::TextProperties>::Run(context);
    DescribeHook<::Aero::RichText>::Run(context);
    DescribeHook<::Aero::SetterBase>::Run(context);
    DescribeHook<::Aero::Setter>::Run(context);
    DescribeHook<::Aero::EventSetter>::Run(context);
    DescribeHook<::Aero::Data::IValueConverter>::Run(context);
    DescribeHook<::Aero::Data::IMultiValueConverter>::Run(context);
    DescribeHook<::Aero::Data::BooleanToVisibilityConverter>::Run(context);
    DescribeHook<::Aero::Data::BindingBase>::Run(context);
    DescribeHook<::Aero::Data::RelativeSource>::Run(context);
    DescribeHook<::Aero::Data::Binding>::Run(context);
    DescribeHook<::Aero::Data::MultiBinding>::Run(context);
    DescribeHook<::Aero::Data::MultiBindingProxy>::Run(context);
    DescribeHook<::Aero::TriggerBase>::Run(context);
    DescribeHook<::Aero::Trigger>::Run(context);
    DescribeHook<::Aero::DataTrigger>::Run(context);
    DescribeHook<::Aero::Condition>::Run(context);
    DescribeHook<::Aero::MultiDataTrigger>::Run(context);
    DescribeHook<::Aero::MultiTrigger>::Run(context);
    DescribeHook<::Aero::Style>::Run(context);
    return {};
}

} // namespace Aero

// ===== Animation keyframe helpers (MetadataSupport) =====

namespace Aero::MetadataSupport {
using Media::Animation::BooleanAnimationUsingKeyFrames;
using Media::Animation::BooleanKeyFrame;
using Media::Animation::ColorAnimationUsingKeyFrames;
using Media::Animation::ColorKeyFrame;
using Media::Animation::DoubleAnimationUsingKeyFrames;
using Media::Animation::DoubleKeyFrame;
using Media::Animation::Int16AnimationUsingKeyFrames;
using Media::Animation::Int16KeyFrame;
using Media::Animation::Int32AnimationUsingKeyFrames;
using Media::Animation::Int32KeyFrame;
using Media::Animation::Int64AnimationUsingKeyFrames;
using Media::Animation::Int64KeyFrame;
using Media::Animation::MatrixAnimationUsingKeyFrames;
using Media::Animation::MatrixKeyFrame;
using Media::Animation::ObjectAnimationUsingKeyFrames;
using Media::Animation::ObjectKeyFrame;
using Media::Animation::PointAnimationUsingKeyFrames;
using Media::Animation::PointKeyFrame;
using Media::Animation::SizeAnimationUsingKeyFrames;
using Media::Animation::SizeKeyFrame;
using Media::Animation::StringAnimationUsingKeyFrames;
using Media::Animation::StringKeyFrame;
using Media::Animation::ThicknessAnimationUsingKeyFrames;
using Media::Animation::ThicknessKeyFrame;
namespace {

void AddDoubleKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<DoubleKeyFrame> retained =
        Base::Ref<DoubleKeyFrame>::TryFromBorrowed(
            static_cast<DoubleKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<DoubleAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearDoubleKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<DoubleAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddPointKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<PointKeyFrame> retained =
        Base::Ref<PointKeyFrame>::TryFromBorrowed(
            static_cast<PointKeyFrame&>(*value));
    if (!retained) return;
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearPointKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<PointAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddThicknessKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ThicknessKeyFrame> retained =
        Base::Ref<ThicknessKeyFrame>::
            TryFromBorrowed(
                static_cast<
                    ThicknessKeyFrame&>(
                        *value));
    if (!retained) {
        return;
    }
    static_cast<ThicknessAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearThicknessKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ThicknessAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddColorKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ColorKeyFrame> retained =
        Base::Ref<ColorKeyFrame>::TryFromBorrowed(
            static_cast<ColorKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ColorAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearColorKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ColorAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddObjectKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<ObjectKeyFrame> retained =
        Base::Ref<ObjectKeyFrame>::TryFromBorrowed(
            static_cast<ObjectKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<ObjectAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearObjectKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ObjectAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddBooleanKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<BooleanKeyFrame> retained =
        Base::Ref<BooleanKeyFrame>::TryFromBorrowed(
            static_cast<BooleanKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<BooleanAnimationUsingKeyFrames&>(
        owner).AddKeyFrame(std::move(retained));
}

void ClearBooleanKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<BooleanAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddInt16KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int16KeyFrame> retained =
        Base::Ref<Int16KeyFrame>::TryFromBorrowed(
            static_cast<Int16KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt16KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int16AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt32KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int32KeyFrame> retained =
        Base::Ref<Int32KeyFrame>::TryFromBorrowed(
            static_cast<Int32KeyFrame&>(*value));
    if (!retained) return;
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .AddKeyFrame(std::move(retained));
}

void ClearInt32KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int32AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddInt64KeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Int64KeyFrame> retained =
        Base::Ref<Int64KeyFrame>::TryFromBorrowed(
            static_cast<Int64KeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Int64AnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearInt64KeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Int64AnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddSizeKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<SizeKeyFrame> retained =
        Base::Ref<SizeKeyFrame>::TryFromBorrowed(
            static_cast<SizeKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<SizeAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearSizeKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<SizeAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddMatrixKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<MatrixKeyFrame> retained =
        Base::Ref<MatrixKeyFrame>::TryFromBorrowed(
            static_cast<MatrixKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MatrixAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearMatrixKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<MatrixAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddStringKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<StringKeyFrame> retained =
        Base::Ref<StringKeyFrame>::TryFromBorrowed(
            static_cast<StringKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<StringAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearStringKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<StringAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

} // namespace
} // namespace Aero::MetadataSupport

// ===== PopulateUiAnimation =====

namespace Aero {
using namespace ::Aero::MetadataSupport;
using namespace ::Aero::Meta;

Base::Result<void> PopulateUiAnimation(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Media::Animation;
    // Prefer public Animation types over Model::* (AnimationEngine.hpp).
    using Media::Animation::ColorAnimation;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimation;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::MatrixAnimation;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::PointAnimation;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::RectAnimation;
    using Media::Animation::RepeatBehavior;
    using Media::Animation::SizeAnimation;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::ThicknessAnimation;
    using Media::Animation::ThicknessKeyFrame;
    // Prefer geometry typedefs over BrushRendering/AnimationModel conversion fns in Animation.
    using ::Aero::Base::Color;
    using ::Aero::Base::Point;
    using ::Aero::Base::Rect;
    using ::Aero::Base::Size;
    using ::Aero::Base::Thickness;
    Register<Duration>(context)
        .ValueSemantics()
        .TextConverter<&Duration::TryParse>();
    Register<TimeSpan>(context)
        .ValueSemantics()
        .TextConverter<&TimeSpan::TryParse>();
    Register<RepeatBehavior>(context)
        .ValueSemantics()
        .TextConverter<&RepeatBehavior::TryParse>();
    Register<KeyTime>(context)
        .ValueSemantics()
        .TextConverter<&KeyTime::TryParse>();
    DescribeHook<::Aero::Media::Animation::Timeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::AnimationTimeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::TimelineGroup>::Run(context);
    DescribeHook<::Aero::Media::Animation::ParallelTimeline>::Run(context);
    DescribeHook<::Aero::Media::Animation::Storyboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::EasingFunctionBase>::Run(context);

#define AERO_EASE_FACTORY(name, kind) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<EasingFunctionBase>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            EasingFunctionBase::Kind::kind); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "SineEase"),
        AERO_EASE_FACTORY(SineEase, Sine));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuadraticEase"),
        AERO_EASE_FACTORY(QuadraticEase, Quadratic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CubicEase"),
        AERO_EASE_FACTORY(CubicEase, Cubic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuarticEase"),
        AERO_EASE_FACTORY(QuarticEase, Quartic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "QuinticEase"),
        AERO_EASE_FACTORY(QuinticEase, Quintic));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "CircleEase"),
        AERO_EASE_FACTORY(CircleEase, Circle));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ExponentialEase"),
        AERO_EASE_FACTORY(ExponentialEase, Exponential));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "PowerEase"),
        AERO_EASE_FACTORY(PowerEase, Power));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BackEase"),
        AERO_EASE_FACTORY(BackEase, Back));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "BounceEase"),
        AERO_EASE_FACTORY(BounceEase, Bounce));
    SetObjectFactory(RegisterAlias<EasingFunctionBase>(context, "ElasticEase"),
        AERO_EASE_FACTORY(ElasticEase, Elastic));
#undef AERO_EASE_FACTORY

#define AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##AnimationBase>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##AnimationBase::GetFrom, &Name##AnimationBase::SetFrom>("From") \
        .Property<CppType, &Name##AnimationBase::GetTo, &Name##AnimationBase::SetTo>("To");

#define AERO_FROM_TO_ANIMATION(Name, CppType) \
    AERO_FROM_TO_BASE(Name, CppType) \
    Register<Name##Animation>(context) \
        .Property<Base::Ref<EasingFunctionBase>, &Name##Animation::GetEasingFunction, &Name##Animation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural) \
        .Factory();
    AERO_FROM_TO_BASE(Double, double)
    Register<DoubleAnimation>(context)
        .Property<double, &DoubleAnimation::GetAccelerationRatio, &DoubleAnimation::SetAccelerationRatio>("AccelerationRatio")
        .Property<double, &DoubleAnimation::GetDecelerationRatio, &DoubleAnimation::SetDecelerationRatio>("DecelerationRatio")
        .Property<Base::Ref<EasingFunctionBase>, &DoubleAnimation::GetEasingFunction, &DoubleAnimation::SetEasingFunction>("EasingFunction", PropertyFlags::Structural)
        .Factory();
    AERO_FROM_TO_ANIMATION(Color, Color)
    AERO_FROM_TO_ANIMATION(Point, Point)
    AERO_FROM_TO_ANIMATION(Rect, Rect)
    AERO_FROM_TO_ANIMATION(Thickness, Base::Thickness)
    AERO_FROM_TO_ANIMATION(Int16, std::int16_t)
    AERO_FROM_TO_ANIMATION(Int32, std::int32_t)
    AERO_FROM_TO_ANIMATION(Int64, std::int64_t)
    AERO_FROM_TO_ANIMATION(Size, Base::Size)
    AERO_FROM_TO_ANIMATION(Matrix, Base::Transform2D)
#undef AERO_FROM_TO_BASE
#undef AERO_FROM_TO_ANIMATION

#define AERO_KEY_FACTORY(Type, name, interpolation) \
    +[]() noexcept -> Base::Result<Base::Ref<Base::Object>> { \
        auto created = Base::MakeRef<Type>( \
            ::Aero::Meta::MakeTypeId(::Aero::Meta::AeroNamespaceUri(), #name), \
            KeyFrameBase::Interpolation::interpolation); \
        if (!created) return created.GetStatus(); \
        return Base::Ref<Base::Object>(std::move(created).Value()); \
    }
#define AERO_KEYFRAME_ALIAS(Name, Kind) \
    SetObjectFactory(RegisterAlias<Name##KeyFrame>(context, #Kind #Name "KeyFrame"), \
        AERO_KEY_FACTORY(Name##KeyFrame, Kind##Name##KeyFrame, Kind));
#define AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_ALIAS(Name, Linear) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_ALIAS(Name, Easing) \
    AERO_KEYFRAME_ALIAS(Name, Spline)
#define AERO_KEYFRAME_VALUE(Name, CppType) \
    Register<Name##KeyFrame>(context, TypeFlags::Abstract) \
        .Property<CppType, &Name##KeyFrame::GetValue, &Name##KeyFrame::SetValue>("Value");
#define AERO_KEYFRAME_COLLECTION(Name) \
    Register<Name##AnimationUsingKeyFrames>(context) \
        .Content<Name##KeyFrame>("KeyFrames", ContentKind::Collection, &Add##Name##KeyFrame, &Clear##Name##KeyFrames) \
        .Factory();
#define AERO_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIASES(Name) \
    AERO_KEYFRAME_COLLECTION(Name)
#define AERO_DISCRETE_KEYFRAMES(Name, CppType) \
    AERO_KEYFRAME_VALUE(Name, CppType) \
    AERO_KEYFRAME_ALIAS(Name, Discrete) \
    AERO_KEYFRAME_COLLECTION(Name)
    DescribeHook<::Aero::Media::Animation::KeyFrameBase>::Run(context);
    AERO_KEYFRAMES(Double, double)
    AERO_KEYFRAMES(Point, Point)
    AERO_KEYFRAMES(Thickness, Thickness)
    AERO_KEYFRAMES(Color, Base::Color)
    AERO_KEYFRAMES(Int16, std::int16_t)
    AERO_KEYFRAMES(Int32, std::int32_t)
    AERO_KEYFRAMES(Int64, std::int64_t)
    AERO_KEYFRAMES(Size, Base::Size)
    AERO_KEYFRAMES(Matrix, Base::Transform2D)
    DescribeHook<::Aero::Media::Animation::ObjectKeyFrame>::Run(context);
    AERO_KEYFRAME_ALIAS(Object, Discrete)
    AERO_KEYFRAME_COLLECTION(Object)
    AERO_DISCRETE_KEYFRAMES(Boolean, bool)
    AERO_DISCRETE_KEYFRAMES(String, Base::String)
#undef AERO_KEYFRAME_ALIAS
#undef AERO_KEYFRAME_ALIASES
#undef AERO_KEYFRAME_VALUE
#undef AERO_KEYFRAME_COLLECTION
#undef AERO_KEYFRAMES
#undef AERO_DISCRETE_KEYFRAMES

    DescribeHook<::Aero::Interactivity::TriggerAction>::Run(context);
    DescribeHook<::Aero::Input::KeyBinding>::Run(context);
    DescribeHook<::Aero::Input::MouseBinding>::Run(context);
    DescribeHook<::Aero::Interactivity::ChangePropertyAction>::Run(context);
    DescribeHook<::Aero::Interactivity::SetFocusAction>::Run(context);
    DescribeHook<::Aero::Interactivity::LaunchUriOrFileAction>::Run(context);
    DescribeHook<::Aero::Interactivity::RemoveElementAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::ControllableStoryboardAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::BeginStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::ControlStoryboardAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::PauseStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::ResumeStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::StopStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::RemoveStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::SeekStoryboard>::Run(context);
    DescribeHook<::Aero::Media::Animation::EventTrigger>::Run(context);
    DescribeHook<::Aero::Media::Animation::TimerTrigger>::Run(context);
    DescribeHook<::Aero::Interactivity::PropertyChangedTrigger>::Run(context);
    DescribeHook<::Aero::Interactivity::KeyTrigger>::Run(context);
    DescribeHook<::Aero::Interactivity::InvokeCommandAction>::Run(context);
    DescribeHook<::Aero::Interactivity::SelectAction>::Run(context);
    DescribeHook<::Aero::Interactivity::SelectAllAction>::Run(context);
    DescribeHook<::Aero::Interactivity::PlaySoundAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::PlayMediaAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::PauseMediaAction>::Run(context);
    DescribeHook<::Aero::Media::Animation::StopMediaAction>::Run(context);
    DescribeHook<::Aero::Interactivity::ComparisonCondition>::Run(context);
    DescribeHook<::Aero::Interactivity::ConditionalExpression>::Run(context);
    DescribeHook<::Aero::Interactivity::ConditionBehavior>::Run(context);
    DescribeHook<::Aero::Media::Animation::StoryboardCompletedTrigger>::Run(context);
    DescribeHook<::Aero::Interactivity::Behavior>::Run(context);
    DescribeHook<::Aero::Interactivity::MouseDragElementBehavior>::Run(context);
    DescribeHook<::Aero::Interactivity::BackgroundEffectBehavior>::Run(context);
    DescribeHook<::Aero::Interactivity::StyleBehaviorCollection>::Run(context);
    DescribeHook<::Aero::Interactivity::StyleTriggerCollection>::Run(context);
    DescribeHook<::Aero::Interactivity::StyleInteraction>::Run(context);
    DescribeHook<::Aero::Interactivity::Interaction>::Run(context);
    return {};
}

} // namespace Aero

// ===== Fill*Metadata (element spine) =====

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
Base::Result<void> FillVisualMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Register<Visual>(context, TypeFlags::Abstract);
    return {};
}
} // namespace Aero::Meta

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
Base::Result<void> FillContentElementMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Register<ContentElement>(context, TypeFlags::Abstract);
    return {};
}
} // namespace Aero::Meta

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
Base::Result<void> FillFrameworkContentElementMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Register<FrameworkContentElement>(context, TypeFlags::Abstract)
        .Property<Base::Ref<ResourceDictionary>, &FrameworkContentElement::SetResources>("Resources", PropertyFlags::Structural)
        .Property(FrameworkContentElement::DataContextProperty, Value::NullObject(TypeOf<Base::Object>()), Inherits)
        .Property(FrameworkContentElement::StyleProperty, Base::Ref<Style>{})
        .Property(FrameworkContentElement::TagProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(FrameworkContentElement::IsEnabledProperty, true, Inherits)
        .Property(FrameworkContentElement::IsMouseOverProperty, false)
        .Property(FrameworkContentElement::CursorProperty, Base::String{}, Inherits)
        .Property(FrameworkContentElement::OverridesDefaultStyleProperty, false);
    return {};
}
} // namespace Aero::Meta

// ---- Fill helpers (single TU use) ----
namespace Aero {
namespace {

void AddUiElementInputBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Input::InputBinding* binding = ::Aero::TryCast<Input::InputBinding>(value.Get());
    if (binding == nullptr) return;
    Base::Ref<Input::InputBinding> retained =
        Base::Ref<Input::InputBinding>::TryFromBorrowed(*binding);
    if (retained) {
        (void)static_cast<UIElement&>(owner).AddInputBinding(
            std::move(retained));
    }
}

void ClearUiElementInputBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<UIElement&>(owner).ClearInputBindings();
}

void AddUiElementCommandBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Input::CommandBinding* binding = ::Aero::TryCast<Input::CommandBinding>(value.Get());
    if (binding == nullptr) return;
    Base::Ref<Input::CommandBinding> retained =
        Base::Ref<Input::CommandBinding>::TryFromBorrowed(*binding);
    if (retained) {
        (void)static_cast<UIElement&>(owner).AddCommandBinding(
            std::move(retained));
    }
}

void ClearUiElementCommandBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<UIElement&>(owner).ClearCommandBindings();
}

} // namespace
} // namespace Aero

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {

using namespace ::Aero::Input;

Base::Result<void> FillUIElementMetadata(
    Registration& context) noexcept {
    Register<UIElement>(context, TypeFlags::Abstract)
        .Event(UIElement::PreviewMouseMoveEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseMoveEvent)
        .Event(UIElement::MouseEnterEvent, RoutingStrategy::Direct)
        .Event(UIElement::MouseLeaveEvent, RoutingStrategy::Direct)
        .Event(UIElement::PreviewMouseDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseDownEvent)
        .Event(UIElement::PreviewMouseLeftButtonDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseLeftButtonDownEvent)
        .Event(UIElement::PreviewMouseUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseUpEvent)
        .Event(UIElement::PreviewMouseWheelEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseWheelEvent)
        .Event(UIElement::PreviewMouseLeftButtonUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::MouseLeftButtonUpEvent)
        .Event(UIElement::PreviewDragEnterEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragEnterEvent)
        .Event(UIElement::PreviewDragLeaveEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragLeaveEvent)
        .Event(UIElement::PreviewDragOverEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DragOverEvent)
        .Event(UIElement::PreviewDropEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::DropEvent)
        .Event(UIElement::GiveFeedbackEvent)
        .Event(UIElement::DragCompletedEvent)
        .Event(UIElement::GotKeyboardFocusEvent)
        .Event(UIElement::LostKeyboardFocusEvent)
        .Event(UIElement::PreviewKeyDownEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::KeyDownEvent)
        .Event(UIElement::PreviewKeyUpEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::KeyUpEvent)
        .Event(UIElement::PreviewTextInputEvent, RoutingStrategy::Tunnel)
        .Event(UIElement::TextInputEvent)
        .Property(
            UIElement::ClipToBoundsProperty,
            FrameworkPropertyMetadata(false, AffectsArrange)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::ClipProperty,
            FrameworkPropertyMetadata(Base::Ref<Geometry>{}, AffectsRender)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::BlendModeProperty,
            FrameworkPropertyMetadata(BlendMode::Normal)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::EffectProperty,
            FrameworkPropertyMetadata(Base::Ref<Effect>{}, AffectsRender)
                .Changed(&OnEffectChanged))
        .Property(
            UIElement::OpacityMaskProperty,
            FrameworkPropertyMetadata(Base::Ref<Brush>{}, AffectsRender)
                .Changed(&OnOpacityMaskChanged))
        .Property(
            UIElement::IsHitTestVisibleProperty,
            true)
        .Property(
            UIElement::VisibilityProperty,
            FrameworkPropertyMetadata(Visibility::Visible, AffectsMeasure)
                .Changed(&OnRenderStateChanged))
        .Property(
            UIElement::IsEnabledProperty,
            true, Inherits | AffectsRender)
        .Property(
            UIElement::AllowDropProperty,
            false)
        .Property(
            UIElement::IsMouseOverProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsPressedProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsKeyboardFocusedProperty,
            false, AffectsRender)
        .Property(
            UIElement::IsKeyboardFocusWithinProperty,
            false, AffectsRender)
        .Property(
            UIElement::FocusableProperty,
            false)
        .AddOwner(
            KeyboardNavigation::IsTabStopProperty,
            false)
        .AddOwner(
            KeyboardNavigation::TabIndexProperty,
            std::uint32_t{0})
        .AddOwner(
            FocusManager::IsFocusScopeProperty,
            false)
        .Property(
            UIElement::OpacityProperty,
            FrameworkPropertyMetadata(1.0)
                .Changed(&OnRenderStateChanged)
                .Validate(&ValidateUnitDouble))
        .Property(
            UIElement::RenderTransformProperty,
            FrameworkPropertyMetadata(Base::Ref<Transform>{}, AffectsRender)
                .Changed(&OnRenderTransformChanged))
        .Property(
            UIElement::Transform3DProperty,
            FrameworkPropertyMetadata(Base::Ref<Media::Transform3D>{}, AffectsRender)
                .Changed(&OnRenderTransformChanged))
        .Property(
            UIElement::RenderTransformOriginProperty,
            FrameworkPropertyMetadata(Point{})
                .Changed(&OnRenderStateChanged))
        .Collection<InputBinding>(
            "InputBindings",
            &AddUiElementInputBinding,
            &ClearUiElementInputBindings)
        .Collection<CommandBinding>(
            "CommandBindings",
            &AddUiElementCommandBinding,
            &ClearUiElementCommandBindings);
    return {};
}

} // namespace Aero::Meta

// ---- Fill helpers (colocated from meta/Support.inl, single TU use) ----
namespace Aero {
namespace {
constexpr double DefaultMaximum = 1.0e12;

class PlaceholderFrameworkElement : public FrameworkElement {
public:
    PlaceholderFrameworkElement() noexcept
        : FrameworkElement(FrameworkElement::StaticTypeId()) {}
};

bool ValidateLength(const Length& length) noexcept {
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateMarginValue(const Thickness& t) noexcept {
    // WPF permits negative margins for overlap and shared-border layouts.
    return IsFinite(t);
}

void AddFrameworkEventTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::EventTrigger> retained =
        Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
            static_cast<Media::Animation::EventTrigger&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        FrameworkElementSeams::AddAuthoredTrigger(
            static_cast<FrameworkElement&>(owner),
            Base::Ref<Base::Object>(std::move(retained))));
}

void ClearFrameworkEventTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<void>(
        FrameworkElementSeams::ClearAuthoredTriggers(
            static_cast<FrameworkElement&>(owner)));
}

void OnLayoutTransformChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
}
} // namespace
} // namespace Aero

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
using namespace ::Aero::Input;
using namespace ::Aero::Media;

Base::Result<void> FillFrameworkElementMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    // WPF TextElement/Control.Foreground defaults to Brushes.Black.
    Base::Ref<Brush> defaultForeground{};
    if (Base::Result<Base::Ref<Brush>> made =
            MakeSolidColorBrush(Color{0.0F, 0.0F, 0.0F, 1.0F})) {
        defaultForeground = std::move(made).Value();
    }
    Register<FrameworkElement>(context)
        .Event(FrameworkElement::LoadedEvent, RoutingStrategy::Direct)
        .Property<
            Base::Ref<ResourceDictionary>,
            &FrameworkElement::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Property(
            FrameworkElement::DataContextProperty,
            Value::NullObject(
                TypeOf<Base::Object>()), Inherits)
        .Property(
            FrameworkElement::FontFamilyProperty,
            Base::Ref<Media::FontFamily>{}, Inherits | AffectsMeasure)
        .Property(
            FrameworkElement::FlowDirectionProperty,
            FlowDirection::LeftToRight, Inherits | AffectsMeasure)
        .Property(
            FrameworkElement::CursorProperty,
            Base::String{}, Inherits)
        .Property(
            FrameworkElement::ForceCursorProperty,
            false)
        .Property(
            FrameworkElement::InputScopeProperty,
            InputScope::Default)
        .Property(
            FrameworkElement::ForegroundProperty,
            defaultForeground, Inherits | AffectsRender)
        .Property(
            FrameworkElement::StyleProperty,
            Base::Ref<Style>{})
        .Property(
            FrameworkElement::TagProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()))
        .Property(
            FrameworkElement::ToolTipProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()))
        .Property(
            FrameworkElement::WidthProperty, Length::Auto(), AffectsMeasure, &ValidateLength)
        .Property(
            FrameworkElement::HeightProperty, Length::Auto(), AffectsMeasure, &ValidateLength)
        .Property(
            FrameworkElement::ActualWidthProperty,
            0.0)
        .Property(
            FrameworkElement::ActualHeightProperty,
            0.0)
        .Property(
            FrameworkElement::MinWidthProperty, 0.0, AffectsMeasure, &::Aero::Base::Validate::NonNegative<double>)
        .Property(
            FrameworkElement::MaxWidthProperty, DefaultMaximum, AffectsMeasure, &::Aero::Base::Validate::NonNegative<double>)
        .Property(
            FrameworkElement::MinHeightProperty, 0.0, AffectsMeasure, &::Aero::Base::Validate::NonNegative<double>)
        .Property(
            FrameworkElement::MaxHeightProperty, DefaultMaximum, AffectsMeasure, &::Aero::Base::Validate::NonNegative<double>)
        .Property(
            FrameworkElement::MarginProperty, Thickness{}, AffectsMeasure, &ValidateMarginValue)
        .Property(
            FrameworkElement::HorizontalAlignmentProperty,
            HorizontalAlignment::Stretch, AffectsArrange)
        .Property(
            FrameworkElement::VerticalAlignmentProperty,
            VerticalAlignment::Stretch, AffectsArrange)
        .Property(
            FrameworkElement::UseLayoutRoundingProperty,
            false, AffectsMeasure)
        .Property(
            FrameworkElement::SnapsToDevicePixelsProperty,
            false, Inherits | AffectsArrange | AffectsRender)
        .Property(
            FrameworkElement::LayoutTransformProperty,
            FrameworkPropertyMetadata(Base::Ref<Transform>{}, AffectsMeasure)
                .Changed(&OnLayoutTransformChanged))
        .Collection<Media::Animation::EventTrigger>(
            "Triggers",
            &AddFrameworkEventTrigger,
            &ClearFrameworkEventTriggers)
        .Factory<PlaceholderFrameworkElement>();
    return {};
}

} // namespace Aero::Meta

// ===== PopulateUiElements / PopulateUiMetadata =====


namespace Aero {

Base::Result<void> PopulateUiElements(
    ::Aero::Meta::Registration& context) noexcept {
    ::Aero::Meta::FillVisualMetadata(context);
    ::Aero::Meta::FillContentElementMetadata(context);
    ::Aero::Meta::FillFrameworkContentElementMetadata(context);
    ::Aero::Meta::FillUIElementMetadata(context);
    ::Aero::Meta::FillFrameworkElementMetadata(context);
    return {};
}

Base::Result<void> PopulateUiMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    PopulateEnumMetadata(context);
    PopulateUiInput(context);
    // Media registers foundational value types such as Point. Resources author
    // Geometry dependency-property defaults that consume those values, so keep
    // Media ahead of Resources in the deterministic metadata bootstrap.
    PopulateUiMedia(context);
    PopulateUiResources(context);
    PopulateUiStyling(context);
    PopulateUiAnimation(context);
    PopulateUiElements(context);
    PopulateInputDevices(context);
    return {};
}

} // namespace Aero

// ===== RegisterControlsMetadata / PopulateControlsMetadata =====

#include "gui/core/TypeRegistryCore.hpp"

// Templates
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>

// Panels
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/VirtualizingWrapPanel.hpp>
#include <Aero/Controls/Grid.hpp>

// Primitives & Buttons
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Ranges.hpp>
#include <Aero/Controls/GridSplitter.hpp>
#include <Aero/Controls/ScrollViewer.hpp>

// Content & Decorators
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/UserControl.hpp>
#include <Aero/Controls/Headers.hpp>
#include <Aero/Controls/Label.hpp>
#include <Aero/Controls/Popup.hpp>

// Items
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>

// Selection
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Selectors.hpp>

// Trees
#include <Aero/Controls/TreeView.hpp>

// Menus
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Separator.hpp>

// Bars & ToolTips
#include <Aero/Controls/ToolBar.hpp>
#include <Aero/Controls/StatusBar.hpp>
#include <Aero/Controls/ToolTip.hpp>

// Text & Media
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Image.hpp>

// Shapes
#include <Aero/Shapes.hpp>

// ListView & GridView
#include <Aero/Controls/GridViews.hpp>
#include <Aero/Controls/ListView.hpp>
#include "gui/core/Describe.hpp"

namespace Aero::Controls {

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept;

Base::Result<void> RegisterControlsMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 30U;
    constexpr Base::StringView name = "Aero.Controls";
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &PopulateControlsMetadata,
        nullptr,
        nullptr});
}

Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    // Describes live next to each control. Installing them records the
    // function; EnsureRegisteredPack then registers bases before derived types.
    static bool describesInstalled = false;
    if (!describesInstalled) {
        describesInstalled = true;
        // Installs private DescribeMetadata hooks. Included inside Aero::Controls.

        AddDescribe<VirtualizingPanel>(&DescribeHook<VirtualizingPanel>::Run);
        AddDescribe<VirtualizingStackPanel>(&DescribeHook<VirtualizingStackPanel>::Run);
        AddDescribe<VirtualizingWrapPanel>(&DescribeHook<VirtualizingWrapPanel>::Run);
        AddDescribe<ControlTemplate>(&DescribeHook<ControlTemplate>::Run);
        AddDescribe<ItemsPanelTemplate>(&DescribeHook<ItemsPanelTemplate>::Run);
        AddDescribe<FrameworkTemplate>(&DescribeHook<FrameworkTemplate>::Run);
        AddDescribe<DataTemplate>(&DescribeHook<DataTemplate>::Run);
        AddDescribe<HierarchicalDataTemplate>(&DescribeHook<HierarchicalDataTemplate>::Run);
        AddDescribe<ToolBar>(&DescribeHook<ToolBar>::Run);
        AddDescribe<ToolBarTray>(&DescribeHook<ToolBarTray>::Run);
        AddDescribe<StatusBar>(&DescribeHook<StatusBar>::Run);
        AddDescribe<ToolTip>(&DescribeHook<ToolTip>::Run);
        AddDescribe<ToolTipService>(&DescribeHook<ToolTipService>::Run);
        AddDescribe<Shapes::Shape>(&DescribeHook<Shapes::Shape>::Run);
        AddDescribe<Shapes::Rectangle>(&DescribeHook<Shapes::Rectangle>::Run);
        AddDescribe<Shapes::Path>(&DescribeHook<Shapes::Path>::Run);
        AddDescribe<Shapes::Line>(&DescribeHook<Shapes::Line>::Run);
        AddDescribe<Shapes::Polygon>(&DescribeHook<Shapes::Polygon>::Run);
        AddDescribe<Shapes::Polyline>(&DescribeHook<Shapes::Polyline>::Run);
        AddDescribe<Primitives::TextBoxBase>(&DescribeHook<Primitives::TextBoxBase>::Run);
        AddDescribe<TextBox>(&DescribeHook<TextBox>::Run);
        AddDescribe<TextBlock>(&DescribeHook<TextBlock>::Run);
        AddDescribe<TreeView>(&DescribeHook<TreeView>::Run);
        AddDescribe<TreeViewItem>(&DescribeHook<TreeViewItem>::Run);
        AddDescribe<Panel>(&DescribeHook<Panel>::Run);
        AddDescribe<StackPanel>(&DescribeHook<StackPanel>::Run);
        AddDescribe<DockPanel>(&DescribeHook<DockPanel>::Run);
        AddDescribe<WrapPanel>(&DescribeHook<WrapPanel>::Run);
        AddDescribe<UniformGrid>(&DescribeHook<UniformGrid>::Run);
        AddDescribe<Canvas>(&DescribeHook<Canvas>::Run);
        AddDescribe<Grid>(&DescribeHook<Grid>::Run);
        AddDescribe<Control>(&DescribeHook<Control>::Run);
        AddDescribe<ContentControl>(&DescribeHook<ContentControl>::Run);
        AddDescribe<HeaderedContentControl>(&DescribeHook<HeaderedContentControl>::Run);
        AddDescribe<Decorator>(&DescribeHook<Decorator>::Run);
        AddDescribe<BulletDecorator>(&DescribeHook<BulletDecorator>::Run);
        AddDescribe<Viewbox>(&DescribeHook<Viewbox>::Run);
        AddDescribe<Border>(&DescribeHook<Border>::Run);
        AddDescribe<ContentPresenter>(&DescribeHook<ContentPresenter>::Run);
        AddDescribe<Expander>(&DescribeHook<Expander>::Run);
        AddDescribe<TabItem>(&DescribeHook<TabItem>::Run);
        AddDescribe<TabControl>(&DescribeHook<TabControl>::Run);
        AddDescribe<Primitives::Popup>(&DescribeHook<Primitives::Popup>::Run);
        AddDescribe<Primitives::Track>(&DescribeHook<Primitives::Track>::Run);
        AddDescribe<Primitives::Thumb>(&DescribeHook<Primitives::Thumb>::Run);
        AddDescribe<Primitives::RangeBase>(&DescribeHook<Primitives::RangeBase>::Run);
        AddDescribe<Primitives::ScrollBar>(&DescribeHook<Primitives::ScrollBar>::Run);
        AddDescribe<Slider>(&DescribeHook<Slider>::Run);
        AddDescribe<TickBar>(&DescribeHook<TickBar>::Run);
        AddDescribe<ProgressBar>(&DescribeHook<ProgressBar>::Run);
        AddDescribe<GridSplitter>(&DescribeHook<GridSplitter>::Run);
        AddDescribe<ScrollContentPresenter>(&DescribeHook<ScrollContentPresenter>::Run);
        AddDescribe<ScrollViewer>(&DescribeHook<ScrollViewer>::Run);
        AddDescribe<Primitives::Selector>(&DescribeHook<Primitives::Selector>::Run);
        AddDescribe<ListBox>(&DescribeHook<ListBox>::Run);
        AddDescribe<ListBoxItem>(&DescribeHook<ListBoxItem>::Run);
        AddDescribe<ComboBox>(&DescribeHook<ComboBox>::Run);
        AddDescribe<ComboBoxItem>(&DescribeHook<ComboBoxItem>::Run);
        AddDescribe<MenuBase>(&DescribeHook<MenuBase>::Run);
        AddDescribe<Menu>(&DescribeHook<Menu>::Run);
        AddDescribe<Page>(&DescribeHook<Page>::Run);
        AddDescribe<MenuItem>(&DescribeHook<MenuItem>::Run);
        AddDescribe<ContextMenu>(&DescribeHook<ContextMenu>::Run);
        AddDescribe<ContextMenuService>(&DescribeHook<ContextMenuService>::Run);
        AddDescribe<PasswordBox>(&DescribeHook<PasswordBox>::Run);
        AddDescribe<Primitives::ButtonBase>(&DescribeHook<Primitives::ButtonBase>::Run);
        AddDescribe<Primitives::RepeatButton>(&DescribeHook<Primitives::RepeatButton>::Run);
        AddDescribe<Primitives::ToggleButton>(&DescribeHook<Primitives::ToggleButton>::Run);
        AddDescribe<RadioButton>(&DescribeHook<RadioButton>::Run);
        AddDescribe<GridViewColumnHeader>(&DescribeHook<GridViewColumnHeader>::Run);
        AddDescribe<GridViewColumn>(&DescribeHook<GridViewColumn>::Run);
        AddDescribe<GridView>(&DescribeHook<GridView>::Run);
        AddDescribe<GridViewHeaderRowPresenter>(&DescribeHook<GridViewHeaderRowPresenter>::Run);
        AddDescribe<GridViewRowPresenter>(&DescribeHook<GridViewRowPresenter>::Run);
        AddDescribe<ListView>(&DescribeHook<ListView>::Run);
        AddDescribe<ListViewItem>(&DescribeHook<ListViewItem>::Run);
        AddDescribe<ItemsControl>(&DescribeHook<ItemsControl>::Run);
        AddDescribe<HeaderedItemsControl>(&DescribeHook<HeaderedItemsControl>::Run);
        AddDescribe<Image>(&DescribeHook<Image>::Run);
    }
    EnsureRegisteredPack<
        FrameworkTemplate,
        ControlTemplate,
        DataTemplate,
        DataTemplateSelector,
        HierarchicalDataTemplate,
        ItemsPanelTemplate,
        Panel,
        StackPanel,
        DockPanel,
        WrapPanel,
        UniformGrid,
        VirtualizingPanel,
        VirtualizingStackPanel,
        VirtualizingWrapPanel,
        Canvas,
        Grid,
        Control,
        ContentControl,
        HeaderedContentControl,
        Decorator,
        Border,
        BulletDecorator,
        Viewbox,
        ContentPresenter,
        Primitives::ButtonBase,
        Button,
        Primitives::RepeatButton,
        Primitives::ToggleButton,
        CheckBox,
        RadioButton,
        Primitives::Thumb,
        Primitives::Track,
        Primitives::RangeBase,
        Primitives::ScrollBar,
        Slider,
        TickBar,
        ProgressBar,
        GridSplitter,
        ScrollContentPresenter,
        ScrollViewer,
        UserControl,
        Page,
        GroupBox,
        Label,
        Expander,
        Primitives::Popup,
        ItemsControl,
        HeaderedItemsControl,
        ItemsPresenter,
        Primitives::Selector,
        ListBox,
        ListBoxItem,
        ComboBox,
        ComboBoxItem,
        TabItem,
        TabControl,
        TabPanel,
        TreeView,
        TreeViewItem,
        Menu,
        MenuItem,
        ContextMenu,
        ContextMenuService,
        Separator,
        ToolBar,
        ToolBarPanel,
        ToolBarOverflowPanel,
        ToolBarTray,
        StatusBar,
        StatusBarItem,
        ToolTip,
        ToolTipService,
        TextBlock,
        Primitives::TextBoxBase,
        TextBox,
        PasswordBox,
        Image,
        Shapes::Shape,
        Shapes::Rectangle,
        Shapes::Ellipse,
        Shapes::Path,
        Shapes::Line,
        Shapes::Polygon,
        Shapes::Polyline,
        GridViewColumnHeader,
        GridViewColumn,
        GridView,
        GridViewHeaderRowPresenter,
        GridViewRowPresenter,
        ListView,
        ListViewItem>(context);
    return {};
}

} // namespace Aero::Controls

// ===== Markup Populate + extension tokens =====

namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;


class DynamicResourceExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        DynamicResourceExtensionToken,
        Base::Object,
        "urn:aero",
        "DynamicResource")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Static")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TypeExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TypeExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Type")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TemplateBindingExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TemplateBindingExtensionToken,
        Base::Object,
        "urn:aero",
        "TemplateBinding")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticResourceExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticResourceExtensionToken,
        Base::Object,
        Meta::AeroNamespaceUri(),
        "StaticResourceExtension")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

// AeroGUI's application samples expose Loc both as a markup extension and as
// an attached Source property.  The token deliberately lives in the normal
// schema so the legacy AeroGUIExtensions namespace resolves to the same type
// as other compatibility extensions.
class LocExtensionToken
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        LocExtensionToken,
        Base::Object,
        "urn:aero",
        "Loc")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Meta::AttachedPropertyRef<
        LocExtensionToken, Base::ResourceUri>
        SourceProperty{"Source"};
};

void AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualState::StaticTypeId()) {
        return;
    }
    static_cast<VisualStateGroup&>(object).AddState(
        Base::Ref<VisualState>::FromBorrowed(
            *static_cast<VisualState*>(value.Get())));
}

void ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(object).ClearStates();
}

void AddGroupTransition(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            VisualTransition::StaticTypeId()) {
        return;
    }
    static_cast<VisualStateGroup&>(object).AddTransition(
        Base::Ref<VisualTransition>::FromBorrowed(
            *static_cast<VisualTransition*>(value.Get())));
}

void ClearGroupTransitions(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualStateGroup&>(
        object).ClearTransitions();
}

[[maybe_unused]] void AddElementVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) {
        return;
    }
    auto& target = static_cast<::Aero::DependencyObject&>(object);
    Base::Ref<VisualStateGroupCollection> valueStore = target.GetValue(VisualStateManager::VisualStateGroupsProperty);
    if (!valueStore) {
        Base::Result<Base::Ref<VisualStateGroupCollection>> created =
            Base::MakeRef<VisualStateGroupCollection>();
        if (!created) return;
        valueStore = std::move(created).Value();
        target.SetValue(
            VisualStateManager::VisualStateGroupsProperty,
            valueStore);
    }
    (void)valueStore->Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearElementVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<::Aero::DependencyObject&>(object).SetValue(
        VisualStateManager::VisualStateGroupsProperty,
        Base::Ref<VisualStateGroupCollection>{});
}

void AddStateContent(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    auto& state =
        static_cast<VisualState&>(object);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        state.AddSetter(value);
        return;
    }
    if (value->RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        state.SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(value.Get())));
        return;
    }
    return;
}

void ClearStateContent(
    Base::Object& object,
    void*) noexcept {
    auto& state =
        static_cast<VisualState&>(object);
    state.ClearSetters();
    state.SetStoryboard({});
}

void SetTransitionStoryboard(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            Media::Animation::Storyboard::StaticTypeId()) {
        return;
    }
    static_cast<VisualTransition&>(
        object).SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(
                    value.Get())));
}

void ClearTransitionStoryboard(
    Base::Object& object,
    void*) noexcept {
    static_cast<VisualTransition&>(object).SetStoryboard({});
}

[[maybe_unused]] void AddVisualStateGroupToCollection(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != VisualStateGroup::StaticTypeId()) return;
    auto& collection = static_cast<VisualStateGroupCollection&>(owner);
    collection.Add(
        Base::Ref<VisualStateGroup>::FromBorrowed(
            *static_cast<VisualStateGroup*>(value.Get())));
}

[[maybe_unused]] void ClearVisualStateGroupCollection(
    Base::Object& owner,
    void*) noexcept {
    static_cast<VisualStateGroupCollection&>(owner).Clear();
}

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status =
        Meta::Register<MarkupExtension>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();
    status =
        Meta::Register<DynamicResourceExtensionToken>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TypeExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<TemplateBindingExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    auto loc = Meta::Register<LocExtensionToken>(
        context,
        TypeFlags::MarkupExtension | TypeFlags::Abstract);
    loc.Property(
        LocExtensionToken::SourceProperty,
        FrameworkPropertyMetadata(Base::ResourceUri{}, Inherits).Changed(
            &LocExtension::OnSourceChanged));
    status = loc.Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            Base::String{})
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<VisualStateGroupCollection>(
        context, TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();

    auto visualStateManager =
        Meta::Register<VisualStateManager>(
            context, TypeFlags::Abstract);
    visualStateManager
        .Property(
            VisualStateManager::VisualStateGroupsProperty,
            FrameworkPropertyMetadata(
                Base::Ref<VisualStateGroupCollection>{})
                .Structural());
    status = visualStateManager.Result();
    if (!status) return status.GetStatus();

    auto stateGroup =
        Meta::Register<VisualStateGroup>(context);
    stateGroup
        .Property(
            "Name",
            &VisualStateGroup::GetName,
            &VisualStateGroup::SetName)
        .Content<VisualState>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .Collection<VisualTransition>(
            "Transitions",
            &AddGroupTransition,
            &ClearGroupTransitions)
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Meta::Register<VisualState>(context);
    state
        .Property(
            "Name",
            &VisualState::GetName,
            &VisualState::SetName)
        .Property<
            Base::Ref<Media::Animation::Storyboard>,
            &VisualState::GetStoryboard,
            &VisualState::SetStoryboard>(
            "Storyboard",
            PropertyFlags::Structural)
        .Content<Base::Object>(
            "Content",
            ContentKind::Collection,
            &AddStateContent,
            &ClearStateContent)
        .Factory();
    status = state.Result();
    if (!status) return status.GetStatus();

    auto transition =
        Meta::Register<VisualTransition>(context);
    transition
        .Property(
            "From",
            &VisualTransition::GetFrom,
            &VisualTransition::SetFrom)
        .Property(
            "To",
            &VisualTransition::GetTo,
            &VisualTransition::SetTo)
        .Property(
            "GeneratedDuration",
            &VisualTransition::GetGeneratedDuration,
            &VisualTransition::SetGeneratedDuration)
        .Property<
            Base::Ref<Media::Animation::EasingFunctionBase>,
            &VisualTransition::GetGeneratedEasingFunction,
            &VisualTransition::SetGeneratedEasingFunction>(
            "GeneratedEasingFunction",
            PropertyFlags::Structural)
        .Content<Media::Animation::Storyboard>(
            "Storyboard",
            ContentKind::Single,
            &SetTransitionStoryboard,
            &ClearTransitionStoryboard)
        .Factory();
    status = transition.Result();
    return status;
}

} // namespace Aero::Markup

// ===== RegisterBuiltIn* entry points =====

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<void> registered =
        Meta::RegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::RegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Controls::RegisterControlsMetadata(domain);
}

Base::Result<void> RegisterBuiltInMarkupModule(
    ::Aero::Meta::Registry& domain) noexcept {
    return Markup::RegisterMarkupMetadata(domain);
}

} // namespace Aero

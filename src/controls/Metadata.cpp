#include <Aero/Controls/Metadata.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Metadata.hpp>

#include <cctype>
#include <cmath>
#include <limits>
#include <utility>

namespace Aero::Controls {
namespace {

Base::Result<Core::Value> GetTemplateVisualTree(
    const Base::Object& object,
    void*) noexcept {
    const Base::Ref<Base::Object>& value =
        static_cast<const ControlTemplate&>(object)
            .AuthoredVisualTree();
    return Core::Value::FromObject(
        Core::TypeOf<Base::Object>(),
        value);
}

Base::Result<void> SetTemplateVisualTree(
    Base::Object& object,
    const Core::Value& value,
    void*) noexcept {
    if (value.Kind() != Core::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate VisualTree expects an object");
    }
    return static_cast<ControlTemplate&>(object)
        .SetAuthoredVisualTree(value.AsObject());
}

template<class T>
Base::Result<void> SetDeferredTemplateVisualTree(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<T&>(object)
        .SetAuthoredVisualTree(value);
}

template<class T>
Base::Result<void> ClearDeferredTemplateVisualTree(
    Base::Object& object,
    void*) noexcept {
    static_cast<T&>(object)
        .ClearAuthoredVisualTree();
    return {};
}

Base::Result<void> AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<ControlTemplate&>(object)
        .TryAddAuthoredVisualStateGroup(value);
}

Base::Result<void> ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<ControlTemplate&>(object)
        .ClearAuthoredVisualStateGroups();
    return {};
}

template<class T>
Base::Result<void> SetTemplateResources(
    Base::Object& object,
    const Core::Value& value,
    void*) noexcept {
    if (value.Kind() != Core::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            Presentation::ResourceDictionary::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template Resources expects a ResourceDictionary");
    }
    auto& target = static_cast<T&>(object).Resources();
    if (target.Size() != 0U ||
        target.MergedDictionaryCount() != 0U ||
        !target.Source().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Template Resources is already assigned");
    }
    auto& source =
        static_cast<Presentation::ResourceDictionary&>(
            *value.AsObject());
    target = std::move(source);
    return {};
}

Core::PropertyRegistration OrdinaryProperty(
    Base::StringView name,
    Core::TypeId type,
    Core::PropertyGetCallback get,
    Core::PropertySetCallback set,
    Core::PropertyFlags flags =
        Core::PropertyFlags::None) noexcept {
    Core::PropertyRegistration registration;
    registration.name = name;
    registration.valueType = type;
    registration.flags = flags;
    registration.access =
        Core::PropertyAccessKind::Ordinary;
    registration.get = get;
    registration.set = set;
    return registration;
}

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

Base::Result<Core::Value> ConvertSelectionMode(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "single")) {
        return Core::Value::FromUnsignedInteger(
            type,
            static_cast<std::uint64_t>(
                SelectionMode::Single));
    }
    if (EqualsAsciiInsensitive(value, "multiple")) {
        return Core::Value::FromUnsignedInteger(
            type,
            static_cast<std::uint64_t>(
                SelectionMode::Multiple));
    }
    if (EqualsAsciiInsensitive(value, "extended")) {
        return Core::Value::FromUnsignedInteger(
            type,
            static_cast<std::uint64_t>(
                SelectionMode::Extended));
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "SelectionMode is invalid");
}

bool ValidateNonnegativeDouble(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Double &&
        std::isfinite(value.AsDouble()) && value.AsDouble() >= 0.0;
}

bool ValidateFiniteDouble(const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Double &&
        std::isfinite(value.AsDouble());
}

bool ValidatePositiveDouble(const Core::Value& value) noexcept {
    return ValidateFiniteDouble(value) &&
        value.AsDouble() > 0.0;
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

bool ValidatePositiveUInt32(
    const Core::Value& value) noexcept {
    return ValidateUInt32(value) &&
        value.AsUnsignedInteger() != 0U;
}

bool ValidateClickModeValue(
    const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            static_cast<std::uint64_t>(ClickMode::Hover);
}

bool ValidateSelectionModeValue(
    const Core::Value& value) noexcept {
    return value.Kind() ==
            Core::ValueKind::UnsignedInteger &&
        value.AsUnsignedInteger() <=
            static_cast<std::uint64_t>(
                SelectionMode::Extended);
}

bool ValidateObjectValue(
    const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Object;
}

Base::Result<Core::Value> CoerceSelectedObject(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Core::Value& value) noexcept {
    const auto& selector =
        static_cast<const Selector&>(object);
    if (!value.IsNullObject() &&
        selector.IndexOfItem(
            value.AsObject().Get()) == UINT32_MAX) {
        return Core::Value::FromObject(
            Core::TypeOf<Base::Object>(), {});
    }
    return value;
}

bool ValidateBooleanValue(
    const Core::Value& value) noexcept {
    return value.Kind() == Core::ValueKind::Boolean;
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

Base::Result<Core::Value> CoerceTextBoxText(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Core::Value& value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> limited =
        validation.SetMaximumLength(
            static_cast<TextBox&>(object).
                MaximumLength());
    if (limited) {
        limited = validation.SetText(
            value.AsString());
    }
    if (!limited) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TextBox text exceeds its UTF-8 or maximum-length contract");
    }
    return value;
}

Base::Result<Core::Value>
CoerceTextBoxMaximumLength(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Core::Value& value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> text =
        validation.SetText(
            static_cast<TextBox&>(object).
                Text());
    if (!text ||
        validation.GraphemeCount() >
            value.AsUnsignedInteger()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TextBox maximum length is shorter than its current text");
    }
    return value;
}

Base::Result<void> SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Panel content child is null");
    }
    return static_cast<Panel&>(owner).AddOwnedChild(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Panel&>(owner).ClearOwnedChildren();
}

Base::Result<void> SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Decorator content child is null");
    }
    return static_cast<Decorator&>(owner).SetOwnedChild(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Decorator&>(owner).SetChild(nullptr);
}

Base::Result<void> SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl content child is null");
    }
    return static_cast<ContentControl&>(owner).SetOwnedContent(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ContentControl&>(owner).SetContent(nullptr);
}

Base::Result<void> SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentPresenter content child is null");
    }
    return static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Presentation::UIElement*>(child.Get()));
}

Base::Result<void> ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

Base::Result<void> AddItemsControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsControl content item is null");
    }
    return static_cast<ItemsControl&>(owner).Items().Add(item);
}

Base::Result<void> ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).Items().Reset();
    return {};
}

} // namespace

Base::Result<void> Detail::PopulateControlsMetadata(
    Core::MetaRegistrationContext& context) noexcept {
    using namespace Aero::Core;
    Base::Result<void> status;

    auto orientation = DescribeEnum<Orientation, std::uint32_t>(context);
    orientation
        .EnumValue("Horizontal", Orientation::Horizontal)
        .EnumValue("Vertical", Orientation::Vertical)
        .TextConverter(&ConvertOrientation);
    status = orientation.Finish();
    if (!status) return status.GetStatus();

    auto clickMode = DescribeEnum<ClickMode, std::uint32_t>(context);
    clickMode
        .EnumValue("Release", ClickMode::Release)
        .EnumValue("Press", ClickMode::Press)
        .EnumValue("Hover", ClickMode::Hover)
        .TextConverter(&ConvertClickMode);
    status = clickMode.Finish();
    if (!status) return status.GetStatus();

    auto selectionMode = DescribeEnum<SelectionMode, std::uint32_t>(context);
    selectionMode
        .EnumValue("Single", SelectionMode::Single)
        .EnumValue("Multiple", SelectionMode::Multiple)
        .EnumValue("Extended", SelectionMode::Extended)
        .TextConverter(&ConvertSelectionMode);
    status = selectionMode.Finish();
    if (!status) return status.GetStatus();

    auto scrollChangedEventArgs = DescribeStruct<ScrollChangedEventArgs>(context);
    status = scrollChangedEventArgs.Finish();
    if (!status) return status.GetStatus();

    auto frameworkTemplate = Describe<FrameworkTemplate>(context, TypeFlags::Abstract);
    frameworkTemplate.Property(OrdinaryProperty(
        "Resources",
        Presentation::ResourceDictionary::StaticTypeId(),
        nullptr,
        &SetTemplateResources<FrameworkTemplate>,
        PropertyFlags::Structural));
    status = frameworkTemplate.Finish();
    if (!status) return status.GetStatus();

    auto controlTemplate = Describe<ControlTemplate>(context);
    controlTemplate
        .Property({
            "TargetType",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property(OrdinaryProperty(
            "VisualTree",
            TypeOf<Base::Object>(),
            &GetTemplateVisualTree,
            &SetTemplateVisualTree,
            PropertyFlags::Structural))
        .Content<Base::Object>(
            "VisualStateGroups",
            ContentKind::Collection,
            &AddTemplateVisualStateGroup,
            &ClearTemplateVisualStateGroups)
        .Factory();
    status = controlTemplate.Finish();
    if (!status) return status.GetStatus();

    auto dataTemplate = Describe<DataTemplate>(context);
    dataTemplate
        .Property({
            "DataType",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .Property(OrdinaryProperty(
            "Resources",
            Presentation::ResourceDictionary::StaticTypeId(),
            nullptr,
            &SetTemplateResources<DataTemplate>,
            PropertyFlags::Structural))
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<DataTemplate>,
            &ClearDeferredTemplateVisualTree<DataTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = dataTemplate.Finish();
    if (!status) return status.GetStatus();

    auto itemsPanelTemplate = Describe<ItemsPanelTemplate>(context);
    itemsPanelTemplate
        .Property(OrdinaryProperty(
            "Resources",
            Presentation::ResourceDictionary::StaticTypeId(),
            nullptr,
            &SetTemplateResources<ItemsPanelTemplate>,
            PropertyFlags::Structural))
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<ItemsPanelTemplate>,
            &ClearDeferredTemplateVisualTree<ItemsPanelTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = itemsPanelTemplate.Finish();
    if (!status) return status.GetStatus();

    auto panel = Describe<Panel>(context, TypeFlags::Abstract);
    panel.Content<Presentation::UIElement>(
        "Children", ContentKind::Collection,
        &SetPanelContent, &ClearPanelContent,
        ContentFlags::Visual);
    status = panel.Finish();
    if (!status) return status.GetStatus();

    auto decorator = Describe<Decorator>(context, TypeFlags::Abstract);
    decorator.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetDecoratorContent, &ClearDecoratorContent,
        ContentFlags::Visual);
    status = decorator.Finish();
    if (!status) return status.GetStatus();

    auto control = Describe<Control>(context, TypeFlags::Abstract);
    control.Property(
            Control::TemplateProperty,
            Value::NullObject(
            ControlTemplate::StaticTypeId()),
            PropertyMetadataFlags::AffectsMeasure);
    status = control.Finish();
    if (!status) return status.GetStatus();

    auto contentControl = Describe<ContentControl>(context, TypeFlags::Abstract);
    contentControl.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetContentControlContent, &ClearContentControlContent,
        ContentFlags::Visual);
    status = contentControl.Finish();
    if (!status) return status.GetStatus();

    auto buttonBase = Describe<ButtonBase>(context, TypeFlags::Abstract);
    if (context.RoutedEvents() != nullptr) {
        buttonBase.Event(
                ButtonBase::ClickEvent,
                RoutingStrategy::Bubble);
    }
    buttonBase
        .Property(
            ButtonBase::ClickModeProperty,
            ClickMode::Release,
            PropertyMetadataFlags::None,
            &ValidateClickModeValue)
        .Property(
            ButtonBase::CommandProperty,
            Value::NullObject(TypeOf<ICommand>()),
            PropertyMetadataFlags::None)
        .Property(
            ButtonBase::CommandParameterProperty,
            Value::NullObject(TypeOf<Base::Object>()),
            PropertyMetadataFlags::None)
        .Property(
            ButtonBase::CommandTargetProperty,
            Value::NullObject(TypeOf<UIElement>()),
            PropertyMetadataFlags::None);
    buttonBase
        .Override(
            UIElement::IsEnabledProperty,
            true,
            PropertyMetadataFlags::Inherits |
                PropertyMetadataFlags::AffectsRender,
            nullptr,
            &CoerceButtonEnabled)
        .Override(UIElement::IsTabStopProperty, true);
    status = buttonBase.Finish();
    if (!status) return status.GetStatus();

    auto button = Describe<Button>(context);
    button.Factory();
    status = button.Finish();
    if (!status) return status.GetStatus();

    auto repeatButton = Describe<RepeatButton>(context);
    repeatButton
        .Property(
            RepeatButton::DelayProperty,
            400U,
            PropertyMetadataFlags::None,
            &ValidateUInt32)
        .Property(
            RepeatButton::IntervalProperty,
            100U,
            PropertyMetadataFlags::None,
            &ValidatePositiveUInt32)
        .Factory();
    repeatButton.Override(
        ButtonBase::ClickModeProperty,
        ClickMode::Press);
    status = repeatButton.Finish();
    if (!status) return status.GetStatus();

    auto toggleButton = Describe<ToggleButton>(context);
    if (context.RoutedEvents() != nullptr) {
        toggleButton
            .Event(
                ToggleButton::CheckedEvent,
                RoutingStrategy::Bubble)
            .Event(
                ToggleButton::UncheckedEvent,
                RoutingStrategy::Bubble)
            .Event(
                ToggleButton::IndeterminateEvent,
                RoutingStrategy::Bubble);
    }
    toggleButton
        .Property(
            ToggleButton::IsCheckedProperty,
            false,
            PropertyMetadataFlags::BindsTwoWayByDefault |
                PropertyMetadataFlags::AffectsRender,
            &ValidateBooleanValue)
        .Property(
            ToggleButton::IsThreeStateProperty,
            false,
            PropertyMetadataFlags::AffectsRender,
            &ValidateBooleanValue)
        .ReadOnlyProperty(
            ToggleButton::IsIndeterminateProperty,
            false,
            PropertyMetadataFlags::AffectsRender,
            &ValidateBooleanValue)
        .Factory();
    status = toggleButton.Finish();
    if (!status) return status.GetStatus();

    auto checkBox = Describe<CheckBox>(context);
    checkBox.Factory();
    status = checkBox.Finish();
    if (!status) return status.GetStatus();

    Base::Result<Value> emptyGroup =
        Value::TryFromString(TypeOf<Base::String>(), {});
    if (!emptyGroup) return emptyGroup.GetStatus();
    auto radioButton = Describe<RadioButton>(context);
    radioButton
        .Property(
            RadioButton::GroupNameProperty,
            emptyGroup.Value(),
            PropertyMetadataFlags::None)
        .Factory();
    status = radioButton.Finish();
    if (!status) return status.GetStatus();

    auto scrollPresenter = Describe<ScrollContentPresenter>(context);
    scrollPresenter.Factory();
    status = scrollPresenter.Finish();
    if (!status) return status.GetStatus();

    auto scrollViewer = Describe<ScrollViewer>(context);
    if (context.RoutedEvents() != nullptr) {
        scrollViewer.Event(
                ScrollViewer::ScrollChangedEvent,
                RoutingStrategy::Bubble);
    }
    scrollViewer
        .ReadOnlyProperty(
            ScrollViewer::HorizontalOffsetProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .ReadOnlyProperty(
            ScrollViewer::VerticalOffsetProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .ReadOnlyProperty(
            ScrollViewer::ExtentWidthProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .ReadOnlyProperty(
            ScrollViewer::ExtentHeightProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .ReadOnlyProperty(
            ScrollViewer::ViewportWidthProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .ReadOnlyProperty(
            ScrollViewer::ViewportHeightProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble)
        .Property(
            ScrollViewer::CanHorizontallyScrollProperty,
            true,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateBooleanValue)
        .Property(
            ScrollViewer::CanVerticallyScrollProperty,
            true,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateBooleanValue)
        .Property(
            ScrollViewer::CanContentScrollProperty,
            false,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateBooleanValue)
        .Factory();
    status = scrollViewer.Finish();
    if (!status) return status.GetStatus();

    auto thumb = Describe<Thumb>(context);
    thumb.Factory();
    status = thumb.Finish();
    if (!status) return status.GetStatus();

    auto track = Describe<Track>(context);
    track
        .Property(
            Track::OrientationProperty,
            Orientation::Vertical,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateOrientationValue)
        .Property(
            Track::MinimumProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateFiniteDouble)
        .Property(
            Track::MaximumProperty,
            1.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateFiniteDouble)
        .Property(
            Track::ValueProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange |
                PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateFiniteDouble)
        .Property(
            Track::ViewportSizeProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateNonnegativeDouble);
    track.Factory();
    status = track.Finish();
    if (!status) return status.GetStatus();

    auto scrollBar = Describe<ScrollBar>(context);
    scrollBar
        .Property(
            ScrollBar::OrientationProperty,
            Orientation::Vertical,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateOrientationValue)
        .Property(
            ScrollBar::MinimumProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateFiniteDouble)
        .Property(
            ScrollBar::MaximumProperty,
            1.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateFiniteDouble)
        .Property(
            ScrollBar::ValueProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange |
                PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateFiniteDouble)
        .Property(
            ScrollBar::ViewportSizeProperty,
            0.0,
            PropertyMetadataFlags::AffectsArrange,
            &ValidateNonnegativeDouble)
        .Property(
            ScrollBar::SmallChangeProperty,
            16.0,
            PropertyMetadataFlags::None,
            &ValidatePositiveDouble)
        .Property(
            ScrollBar::LargeChangeProperty,
            0.0,
            PropertyMetadataFlags::None,
            &ValidateNonnegativeDouble);
    scrollBar.Factory();
    status = scrollBar.Finish();
    if (!status) return status.GetStatus();

    auto itemContainer = Describe<ItemContainer>(context);
    itemContainer.Factory();
    status = itemContainer.Finish();
    if (!status) return status.GetStatus();

    auto itemsControl = Describe<ItemsControl>(context);
    itemsControl.ReadOnlyProperty(
            ItemsControl::ItemCountProperty,
            0U,
            PropertyMetadataFlags::None,
            &ValidateUInt32)
        .Content<Base::Object>(
            "Items", ContentKind::Collection,
            &AddItemsControlItem, &ClearItemsControlItems)
        .Factory();
    status = itemsControl.Finish();
    if (!status) return status.GetStatus();

    auto itemsPresenter = Describe<ItemsPresenter>(context);
    itemsPresenter.Factory();
    status = itemsPresenter.Finish();
    if (!status) return status.GetStatus();

    auto selector = Describe<Selector>(context, TypeFlags::Abstract);
    selector
        .Property(
            Selector::SelectionModeProperty,
            SelectionMode::Single,
            PropertyMetadataFlags::None,
            &ValidateSelectionModeValue)
        .Property(
            Selector::SelectedIndexProperty,
            UINT32_MAX,
            PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateUInt32)
        .Property(
            Selector::SelectedItemProperty,
            Value::FromObject(
                TypeOf<Base::Object>(), {}),
            PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateObjectValue,
            &CoerceSelectedObject)
        .Property(
            Selector::SelectedValueProperty,
            Value::FromObject(
                TypeOf<Base::Object>(), {}),
            PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateObjectValue,
            &CoerceSelectedObject);
    status = selector.Finish();
    if (!status) return status.GetStatus();

    auto listBox = Describe<ListBox>(context);
    listBox.Factory();
    status = listBox.Finish();
    if (!status) return status.GetStatus();

    auto listBoxItem = Describe<ListBoxItem>(context);
    listBoxItem
        .Property(
            ListBoxItem::IsSelectedProperty,
            false,
            PropertyMetadataFlags::AffectsRender |
                PropertyMetadataFlags::BindsTwoWayByDefault,
            &ValidateBooleanValue)
        .Factory();
    status = listBoxItem.Finish();
    if (!status) return status.GetStatus();

    listBoxItem.Override(
        Presentation::UIElement::IsTabStopProperty, true);

    auto userControl = Describe<UserControl>(context);
    userControl.Factory();
    status = userControl.Finish();
    if (!status) return status.GetStatus();

    auto stackPanel = Describe<StackPanel>(context);
    stackPanel
        .Property(
            StackPanel::OrientationProperty,
            Orientation::Vertical,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateOrientationValue)
        .Factory();
    status = stackPanel.Finish();
    if (!status) return status.GetStatus();

    auto virtualizingStackPanel = Describe<VirtualizingStackPanel>(context);
    virtualizingStackPanel
        .Property(
            VirtualizingStackPanel::OrientationProperty,
            Orientation::Vertical,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateOrientationValue)
        .Property(
            VirtualizingStackPanel::OverscanCountProperty,
            2U,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateUInt32)
        .Property(
            VirtualizingStackPanel::
                EstimatedItemExtentProperty,
            24.0,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidatePositiveDouble)
        .Factory();
    status = virtualizingStackPanel.Finish();
    if (!status) return status.GetStatus();

    auto canvas = Describe<Canvas>(context);
    canvas
        .AttachedProperty(
            Canvas::LeftProperty,
            0.0,
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .AttachedProperty(
            Canvas::TopProperty,
            0.0,
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateFiniteDouble)
        .Factory();
    status = canvas.Finish();
    if (!status) return status.GetStatus();

    auto grid = Describe<Grid>(context);
    grid
        .AttachedProperty(
            Grid::RowProperty,
            0U,
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateUInt32)
        .AttachedProperty(
            Grid::ColumnProperty,
            0U,
            PropertyMetadataFlags::AffectsParentMeasure,
            &ValidateUInt32)
        .Factory();
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

    auto border = Describe<Border>(context);
    border
        .Property(
            Border::BackgroundProperty,
            transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Property(
            Border::BorderBrushProperty,
            transparentValue.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Property(
            Border::BorderThicknessProperty,
            0.0,
            PropertyMetadataFlags::AffectsRender,
            &ValidateNonnegativeDouble)
        .Property(
            Border::PaddingProperty,
            padding.Value(),
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateThicknessValue)
        .Factory();
    status = border.Finish();
    if (!status) return status.GetStatus();

    auto textBlock = Describe<TextBlock>(context);
    Base::Result<Value> text =
        Value::TryFromString(TypeOf<Base::String>(), {});
    if (!text) return text.GetStatus();
    const Presentation::Color black{0.0F, 0.0F, 0.0F, 1.0F};
    Base::Result<Value> foreground =
        context.Values().TryCreateValue(
            TypeOf<Presentation::Color>(), &black);
    if (!foreground) return foreground.GetStatus();
    textBlock
        .Property(
            TextBlock::TextProperty,
            text.Value(),
            PropertyMetadataFlags::AffectsMeasure)
        .Property(
            TextBlock::ForegroundProperty,
            foreground.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Factory();
    status = textBlock.Finish();
    if (!status) return status.GetStatus();

    auto textBox = Describe<TextBox>(context);
    const Presentation::Color selection{
        0.18F, 0.48F, 0.95F, 0.45F};
    Base::Result<Value> selectionBrush =
        context.Values().TryCreateValue(
            TypeOf<Presentation::Color>(),
            &selection);
    if (!selectionBrush) {
        return selectionBrush.GetStatus();
    }
    textBox
        .Property(
            TextBox::TextProperty,
            text.Value(),
            PropertyMetadataFlags::AffectsMeasure |
                PropertyMetadataFlags::
                    BindsTwoWayByDefault,
            nullptr,
            &CoerceTextBoxText)
        .Property(
            TextBox::IsReadOnlyProperty,
            false,
            PropertyMetadataFlags::None,
            &ValidateBooleanValue)
        .Property(
            TextBox::MaximumLengthProperty,
            UINT32_MAX,
            PropertyMetadataFlags::None,
            &ValidateUInt32,
            &CoerceTextBoxMaximumLength)
        .Property(
            TextBox::AcceptsReturnProperty,
            false,
            PropertyMetadataFlags::AffectsMeasure,
            &ValidateBooleanValue)
        .Property(
            TextBox::ForegroundProperty,
            foreground.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Property(
            TextBox::SelectionBrushProperty,
            selectionBrush.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Property(
            TextBox::CaretBrushProperty,
            foreground.Value(),
            PropertyMetadataFlags::AffectsRender,
            &ValidateColorValue)
        .Factory();
    status = textBox.Finish();
    if (!status) return status.GetStatus();
    textBox.Override(UIElement::IsTabStopProperty, true);

    auto contentPresenter = Describe<ContentPresenter>(context);
    contentPresenter.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetContentPresenterContent, &ClearContentPresenterContent,
        ContentFlags::Visual)
        .Factory();
    return contentPresenter.Finish();
}

} // namespace Aero::Controls

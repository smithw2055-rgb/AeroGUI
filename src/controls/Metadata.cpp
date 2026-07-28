#include <Aero/Controls/Metadata.hpp>

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>

#include <cmath>
#include <utility>

namespace Aero::Controls {
namespace {

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

Core::TypeReference GetControlTemplateTargetType(
    const ControlTemplate& value) noexcept {
    return {value.TargetType()};
}

Base::Result<void> SetControlTemplateTargetType(
    ControlTemplate& target,
    Core::TypeReference value) noexcept {
    return target.TrySetTargetType(value.type);
}

Core::TypeReference GetDataTemplateType(
    const DataTemplate& value) noexcept {
    return {value.DataType()};
}

Base::Result<void> SetDataTemplateType(
    DataTemplate& target,
    Core::TypeReference value) noexcept {
    return target.SetDataType(value.type);
}

bool ValidateThicknessValue(
    const Presentation::Thickness& thickness) noexcept {
    return Presentation::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

bool ValidateColorValue(
    const Presentation::Color& color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}

Base::Result<Base::Ref<Base::Object>> CoerceSelectedObject(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::Ref<Base::Object>& value) noexcept {
    const auto& selector =
        static_cast<const Selector&>(object);
    if (value &&
        selector.IndexOfItem(value.Get()) == UINT32_MAX) {
        return Base::Ref<Base::Object>{};
    }
    return value;
}

Base::Result<bool> CoerceButtonEnabled(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const bool& value) noexcept {
    return value &&
        static_cast<ButtonBase&>(object).IsCommandEnabled();
}

Base::Result<Base::String> CoerceTextBoxText(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::String& value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> limited =
        validation.SetMaximumLength(
            static_cast<TextBox&>(object).
                MaximumLength());
    if (limited) {
        limited = validation.SetText(
            value.View());
    }
    if (!limited) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TextBox text exceeds its UTF-8 or maximum-length contract");
    }
    return value;
}

Base::Result<std::uint32_t>
CoerceTextBoxMaximumLength(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const std::uint32_t& value) noexcept {
    Text::EditableTextModel validation;
    Base::Result<void> text =
        validation.SetText(
            static_cast<TextBox&>(object).
                Text());
    if (!text ||
        validation.GraphemeCount() >
            value) {
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
    Core::MetadataContext& context) noexcept {
    using namespace Aero::Core;
    Base::Result<void> status;

    auto orientation = Describe<Orientation>(context);
    orientation
        .Value("Horizontal", Orientation::Horizontal)
        .Value("Vertical", Orientation::Vertical);
    status = orientation.Result();
    if (!status) return status.GetStatus();

    auto clickMode = Describe<ClickMode>(context);
    clickMode
        .Value("Release", ClickMode::Release)
        .Value("Press", ClickMode::Press)
        .Value("Hover", ClickMode::Hover);
    status = clickMode.Result();
    if (!status) return status.GetStatus();

    auto selectionMode = Describe<SelectionMode>(context);
    selectionMode
        .Value("Single", SelectionMode::Single)
        .Value("Multiple", SelectionMode::Multiple)
        .Value("Extended", SelectionMode::Extended);
    status = selectionMode.Result();
    if (!status) return status.GetStatus();

    status = Describe<ScrollChangedEventArgs>(context).Result();
    if (!status) return status.GetStatus();

    auto frameworkTemplate = Describe<FrameworkTemplate>(
        context, TypeFlags::Abstract);
    frameworkTemplate
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &FrameworkTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural);
    status = frameworkTemplate.Result();
    if (!status) return status.GetStatus();

    auto controlTemplate = Describe<ControlTemplate>(context);
    controlTemplate
        .Property<
            Core::TypeReference,
            &GetControlTemplateTargetType,
            &SetControlTemplateTargetType>(
            "TargetType",
            PropertyFlags::None)
        .Property(
            "VisualTree",
            &ControlTemplate::AuthoredVisualTree,
            &ControlTemplate::SetAuthoredVisualTree,
            PropertyFlags::Structural)
        .Content<Base::Object>(
            "VisualStateGroups",
            ContentKind::Collection,
            &AddTemplateVisualStateGroup,
            &ClearTemplateVisualStateGroups)
        .Factory();
    status = controlTemplate.Result();
    if (!status) return status.GetStatus();

    auto dataTemplate = Describe<DataTemplate>(context);
    dataTemplate
        .Property<
            Core::TypeReference,
            &GetDataTemplateType,
            &SetDataTemplateType>(
            "DataType",
            PropertyFlags::None)
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &DataTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<DataTemplate>,
            &ClearDeferredTemplateVisualTree<DataTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = dataTemplate.Result();
    if (!status) return status.GetStatus();

    auto itemsPanelTemplate =
        Describe<ItemsPanelTemplate>(context);
    itemsPanelTemplate
        .Property<
            Base::Ref<Presentation::ResourceDictionary>,
            &ItemsPanelTemplate::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Content<Base::Object>(
            "VisualTree",
            ContentKind::Single,
            &SetDeferredTemplateVisualTree<ItemsPanelTemplate>,
            &ClearDeferredTemplateVisualTree<ItemsPanelTemplate>,
            ContentFlags::Visual)
        .Factory();
    status = itemsPanelTemplate.Result();
    if (!status) return status.GetStatus();

    auto panel = Describe<Panel>(
        context, TypeFlags::Abstract);
    panel.Content<Presentation::UIElement>(
        "Children", ContentKind::Collection,
        &SetPanelContent, &ClearPanelContent,
        ContentFlags::Visual);
    status = panel.Result();
    if (!status) return status.GetStatus();

    auto decorator = Describe<Decorator>(
        context, TypeFlags::Abstract);
    decorator.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetDecoratorContent, &ClearDecoratorContent,
        ContentFlags::Visual);
    status = decorator.Result();
    if (!status) return status.GetStatus();

    auto control = Describe<Control>(
        context, TypeFlags::Abstract);
    control.Property(
        Control::TemplateProperty,
        PropertyOptions(Base::Ref<ControlTemplate>{})
            .AffectsMeasure());
    status = control.Result();
    if (!status) return status.GetStatus();

    auto contentControl = Describe<ContentControl>(
        context, TypeFlags::Abstract);
    contentControl.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetContentControlContent, &ClearContentControlContent,
        ContentFlags::Visual);
    status = contentControl.Result();
    if (!status) return status.GetStatus();

    auto buttonBase = Describe<ButtonBase>(
        context, TypeFlags::Abstract);
    buttonBase
        .Event(ButtonBase::ClickEvent)
        .Property(
            ButtonBase::ClickModeProperty,
            PropertyOptions(ClickMode::Release))
        .Property(
            ButtonBase::CommandProperty,
            PropertyOptions(Base::Ref<ICommand>{}))
        .Property(
            ButtonBase::CommandParameterProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Property(
            ButtonBase::CommandTargetProperty,
            PropertyOptions(Base::Ref<UIElement>{}))
        .Override(
            UIElement::IsEnabledProperty,
            PropertyOptions(true)
                .Inherits()
                .AffectsRender()
                .Coerce(&CoerceButtonEnabled))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true));
    status = buttonBase.Result();
    if (!status) return status.GetStatus();

    status = Describe<Button>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto repeatButton = Describe<RepeatButton>(context);
    repeatButton
        .Property(
            RepeatButton::DelayProperty,
            PropertyOptions(std::uint32_t{400}))
        .Property(
            RepeatButton::IntervalProperty,
            PropertyOptions(std::uint32_t{100})
                .Validate(&Validate::Positive<std::uint32_t>))
        .Override(
            ButtonBase::ClickModeProperty,
            PropertyOptions(ClickMode::Press))
        .Factory();
    status = repeatButton.Result();
    if (!status) return status.GetStatus();

    auto toggleButton = Describe<ToggleButton>(context);
    toggleButton
        .Event(ToggleButton::CheckedEvent)
        .Event(ToggleButton::UncheckedEvent)
        .Event(ToggleButton::IndeterminateEvent)
        .Property(
            ToggleButton::IsCheckedProperty,
            PropertyOptions(false)
                .BindsTwoWayByDefault()
                .AffectsRender())
        .Property(
            ToggleButton::IsThreeStateProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Property(
            ToggleButton::IsIndeterminateProperty,
            PropertyOptions(false)
                .AffectsRender())
        .Factory();
    status = toggleButton.Result();
    if (!status) return status.GetStatus();

    status = Describe<CheckBox>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto radioButton = Describe<RadioButton>(context);
    radioButton
        .Property(
            RadioButton::GroupNameProperty,
            PropertyOptions(Base::String{}))
        .Factory();
    status = radioButton.Result();
    if (!status) return status.GetStatus();

    status = Describe<ScrollContentPresenter>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto scrollViewer = Describe<ScrollViewer>(context);
    scrollViewer
        .Event(ScrollViewer::ScrollChangedEvent)
        .Property(
            ScrollViewer::HorizontalOffsetProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::VerticalOffsetProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ExtentWidthProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ExtentHeightProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ViewportWidthProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::ViewportHeightProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollViewer::CanHorizontallyScrollProperty,
            PropertyOptions(true)
                .AffectsMeasure())
        .Property(
            ScrollViewer::CanVerticallyScrollProperty,
            PropertyOptions(true)
                .AffectsMeasure())
        .Property(
            ScrollViewer::CanContentScrollProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Factory();
    status = scrollViewer.Result();
    if (!status) return status.GetStatus();

    status = Describe<Thumb>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto track = Describe<Track>(context);
    track
        .Property(
            Track::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            Track::MinimumProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::MaximumProperty,
            PropertyOptions(1.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::ValueProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .BindsTwoWayByDefault()
                .Validate(&Validate::Finite<double>))
        .Property(
            Track::ViewportSizeProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::NonNegative<double>))
        .Factory();
    status = track.Result();
    if (!status) return status.GetStatus();

    auto scrollBar = Describe<ScrollBar>(context);
    scrollBar
        .Property(
            ScrollBar::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            ScrollBar::MinimumProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            ScrollBar::MaximumProperty,
            PropertyOptions(1.0)
                .AffectsArrange()
                .Validate(&Validate::Finite<double>))
        .Property(
            ScrollBar::ValueProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .BindsTwoWayByDefault()
                .Validate(&Validate::Finite<double>))
        .Property(
            ScrollBar::ViewportSizeProperty,
            PropertyOptions(0.0)
                .AffectsArrange()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            ScrollBar::SmallChangeProperty,
            PropertyOptions(16.0)
                .Validate(&Validate::Positive<double>))
        .Property(
            ScrollBar::LargeChangeProperty,
            PropertyOptions(0.0)
                .Validate(&Validate::NonNegative<double>))
        .Factory();
    status = scrollBar.Result();
    if (!status) return status.GetStatus();

    status = Describe<ItemContainer>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto itemsControl = Describe<ItemsControl>(context);
    itemsControl
        .Property(
            ItemsControl::ItemCountProperty,
            PropertyOptions(std::uint32_t{0}))
        .Content<Base::Object>(
            "Items", ContentKind::Collection,
            &AddItemsControlItem, &ClearItemsControlItems)
        .Factory();
    status = itemsControl.Result();
    if (!status) return status.GetStatus();

    status = Describe<ItemsPresenter>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto selector = Describe<Selector>(
        context, TypeFlags::Abstract);
    selector
        .Property(
            Selector::SelectionModeProperty,
            PropertyOptions(SelectionMode::Single))
        .Property(
            Selector::SelectedIndexProperty,
            PropertyOptions(UINT32_MAX)
                .BindsTwoWayByDefault())
        .Property(
            Selector::SelectedItemProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .BindsTwoWayByDefault()
                .Coerce(&CoerceSelectedObject))
        .Property(
            Selector::SelectedValueProperty,
            PropertyOptions(Base::Ref<Base::Object>{})
                .BindsTwoWayByDefault()
                .Coerce(&CoerceSelectedObject));
    status = selector.Result();
    if (!status) return status.GetStatus();

    status = Describe<ListBox>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto listBoxItem = Describe<ListBoxItem>(context);
    listBoxItem
        .Property(
            ListBoxItem::IsSelectedProperty,
            PropertyOptions(false)
                .AffectsRender()
                .BindsTwoWayByDefault())
        .Override(
            Presentation::UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = listBoxItem.Result();
    if (!status) return status.GetStatus();

    status = Describe<UserControl>(context)
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto stackPanel = Describe<StackPanel>(context);
    stackPanel
        .Property(
            StackPanel::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Factory();
    status = stackPanel.Result();
    if (!status) return status.GetStatus();

    auto virtualizingStackPanel =
        Describe<VirtualizingStackPanel>(context);
    virtualizingStackPanel
        .Property(
            VirtualizingStackPanel::OrientationProperty,
            PropertyOptions(Orientation::Vertical)
                .AffectsMeasure())
        .Property(
            VirtualizingStackPanel::OverscanCountProperty,
            PropertyOptions(std::uint32_t{2})
                .AffectsMeasure())
        .Property(
            VirtualizingStackPanel::
                EstimatedItemExtentProperty,
            PropertyOptions(24.0)
                .AffectsMeasure()
                .Validate(&Validate::Positive<double>))
        .Factory();
    status = virtualizingStackPanel.Result();
    if (!status) return status.GetStatus();

    auto canvas = Describe<Canvas>(context);
    canvas
        .Property(
            Canvas::LeftProperty,
            PropertyOptions(0.0)
                .AffectsParentMeasure()
                .Validate(&Validate::Finite<double>))
        .Property(
            Canvas::TopProperty,
            PropertyOptions(0.0)
                .AffectsParentMeasure()
                .Validate(&Validate::Finite<double>))
        .Factory();
    status = canvas.Result();
    if (!status) return status.GetStatus();

    auto grid = Describe<Grid>(context);
    grid
        .Property(
            Grid::RowProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsParentMeasure())
        .Property(
            Grid::ColumnProperty,
            PropertyOptions(std::uint32_t{0})
                .AffectsParentMeasure())
        .Factory();
    status = grid.Result();
    if (!status) return status.GetStatus();

    auto border = Describe<Border>(context);
    border
        .Property(
            Border::BackgroundProperty,
            PropertyOptions(Presentation::Color{})
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            Border::BorderBrushProperty,
            PropertyOptions(Presentation::Color{})
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            Border::BorderThicknessProperty,
            PropertyOptions(0.0)
                .AffectsRender()
                .Validate(&Validate::NonNegative<double>))
        .Property(
            Border::PaddingProperty,
            PropertyOptions(Presentation::Thickness{})
                .AffectsMeasure()
                .Validate(&ValidateThicknessValue))
        .Factory();
    status = border.Result();
    if (!status) return status.GetStatus();

    const Presentation::Color black{0.0F, 0.0F, 0.0F, 1.0F};
    auto textBlock = Describe<TextBlock>(context);
    textBlock
        .Property(
            TextBlock::TextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure())
        .Property(
            TextBlock::ForegroundProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Factory();
    status = textBlock.Result();
    if (!status) return status.GetStatus();

    const Presentation::Color selection{
        0.18F, 0.48F, 0.95F, 0.45F};
    auto textBox = Describe<TextBox>(context);
    textBox
        .Property(
            TextBox::TextProperty,
            PropertyOptions(Base::String{})
                .AffectsMeasure()
                .BindsTwoWayByDefault()
                .Coerce(&CoerceTextBoxText))
        .Property(
            TextBox::IsReadOnlyProperty,
            PropertyOptions(false))
        .Property(
            TextBox::MaximumLengthProperty,
            PropertyOptions(UINT32_MAX)
                .Coerce(&CoerceTextBoxMaximumLength))
        .Property(
            TextBox::AcceptsReturnProperty,
            PropertyOptions(false)
                .AffectsMeasure())
        .Property(
            TextBox::ForegroundProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            TextBox::SelectionBrushProperty,
            PropertyOptions(selection)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Property(
            TextBox::CaretBrushProperty,
            PropertyOptions(black)
                .AffectsRender()
                .Validate(&ValidateColorValue))
        .Override(
            UIElement::IsTabStopProperty,
            PropertyOptions(true))
        .Factory();
    status = textBox.Result();
    if (!status) return status.GetStatus();

    auto contentPresenter = Describe<ContentPresenter>(context);
    contentPresenter.Content<Presentation::UIElement>(
        "Content", ContentKind::Single,
        &SetContentPresenterContent, &ClearContentPresenterContent,
        ContentFlags::Visual)
        .Factory();
    return contentPresenter.Result();
}

} // namespace Aero::Controls

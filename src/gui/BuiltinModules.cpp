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


// ===== PopulateUiMedia =====

namespace Aero {
using namespace ::Aero::Meta;
using namespace ::Aero::Input;

Base::Result<void> PopulateUiMedia(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status;
    DescribeHook<::Aero::Length>::Run(context);
    status = PopulateMediaValueTypes(context);
    if (!status) return status.GetStatus();
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

// ===== PopulateUiAnimation =====

namespace Aero {

Base::Result<void> PopulateUiAnimation(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status = PopulateAnimationTypes(context);
    if (!status) return status.GetStatus();
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

// ===== PopulateUiElements / PopulateUiMetadata =====


namespace Aero {

Base::Result<void> PopulateUiElements(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::Media::Visual>::Run(context);
    DescribeHook<::Aero::ContentElement>::Run(context);
    DescribeHook<::Aero::FrameworkContentElement>::Run(context);
    DescribeHook<::Aero::UIElement>::Run(context);
    DescribeHook<::Aero::FrameworkElement>::Run(context);
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
    // Hand-ordered DescribeHook::Run: bases before derived (BaseType walk).
    DescribeHook<FrameworkTemplate>::Run(context);
    DescribeHook<ControlTemplate>::Run(context);
    DescribeHook<DataTemplate>::Run(context);
    DescribeHook<DataTemplateSelector>::Run(context);
    DescribeHook<HierarchicalDataTemplate>::Run(context);
    DescribeHook<ItemsPanelTemplate>::Run(context);
    DescribeHook<Panel>::Run(context);
    DescribeHook<StackPanel>::Run(context);
    DescribeHook<DockPanel>::Run(context);
    DescribeHook<WrapPanel>::Run(context);
    DescribeHook<UniformGrid>::Run(context);
    DescribeHook<VirtualizingPanel>::Run(context);
    DescribeHook<VirtualizingStackPanel>::Run(context);
    DescribeHook<VirtualizingWrapPanel>::Run(context);
    DescribeHook<Canvas>::Run(context);
    DescribeHook<Grid>::Run(context);
    DescribeHook<Control>::Run(context);
    DescribeHook<ContentControl>::Run(context);
    DescribeHook<HeaderedContentControl>::Run(context);
    DescribeHook<Decorator>::Run(context);
    DescribeHook<Border>::Run(context);
    DescribeHook<BulletDecorator>::Run(context);
    DescribeHook<Viewbox>::Run(context);
    DescribeHook<ContentPresenter>::Run(context);
    DescribeHook<Primitives::ButtonBase>::Run(context);
    DescribeHook<Button>::Run(context);
    DescribeHook<Primitives::RepeatButton>::Run(context);
    DescribeHook<Primitives::ToggleButton>::Run(context);
    DescribeHook<CheckBox>::Run(context);
    DescribeHook<RadioButton>::Run(context);
    DescribeHook<Primitives::Thumb>::Run(context);
    DescribeHook<Primitives::Track>::Run(context);
    DescribeHook<Primitives::RangeBase>::Run(context);
    DescribeHook<Primitives::ScrollBar>::Run(context);
    DescribeHook<Slider>::Run(context);
    DescribeHook<TickBar>::Run(context);
    DescribeHook<ProgressBar>::Run(context);
    DescribeHook<GridSplitter>::Run(context);
    DescribeHook<ScrollContentPresenter>::Run(context);
    DescribeHook<ScrollViewer>::Run(context);
    DescribeHook<UserControl>::Run(context);
    DescribeHook<Page>::Run(context);
    DescribeHook<GroupBox>::Run(context);
    DescribeHook<Label>::Run(context);
    DescribeHook<Expander>::Run(context);
    DescribeHook<Primitives::Popup>::Run(context);
    DescribeHook<ItemsControl>::Run(context);
    DescribeHook<HeaderedItemsControl>::Run(context);
    DescribeHook<ItemsPresenter>::Run(context);
    DescribeHook<Primitives::Selector>::Run(context);
    DescribeHook<ListBox>::Run(context);
    DescribeHook<ListBoxItem>::Run(context);
    DescribeHook<ComboBox>::Run(context);
    DescribeHook<ComboBoxItem>::Run(context);
    DescribeHook<TabItem>::Run(context);
    DescribeHook<TabControl>::Run(context);
    DescribeHook<TabPanel>::Run(context);
    DescribeHook<TreeView>::Run(context);
    DescribeHook<TreeViewItem>::Run(context);
    DescribeHook<MenuBase>::Run(context);
    DescribeHook<Menu>::Run(context);
    DescribeHook<MenuItem>::Run(context);
    DescribeHook<ContextMenu>::Run(context);
    DescribeHook<ContextMenuService>::Run(context);
    DescribeHook<Separator>::Run(context);
    DescribeHook<ToolBar>::Run(context);
    DescribeHook<ToolBarPanel>::Run(context);
    DescribeHook<ToolBarOverflowPanel>::Run(context);
    DescribeHook<ToolBarTray>::Run(context);
    DescribeHook<StatusBar>::Run(context);
    DescribeHook<StatusBarItem>::Run(context);
    DescribeHook<ToolTip>::Run(context);
    DescribeHook<ToolTipService>::Run(context);
    DescribeHook<TextBlock>::Run(context);
    DescribeHook<Primitives::TextBoxBase>::Run(context);
    DescribeHook<TextBox>::Run(context);
    DescribeHook<PasswordBox>::Run(context);
    DescribeHook<Image>::Run(context);
    DescribeHook<Shapes::Shape>::Run(context);
    DescribeHook<Shapes::Rectangle>::Run(context);
    DescribeHook<Shapes::Ellipse>::Run(context);
    DescribeHook<Shapes::Path>::Run(context);
    DescribeHook<Shapes::Line>::Run(context);
    DescribeHook<Shapes::Polygon>::Run(context);
    DescribeHook<Shapes::Polyline>::Run(context);
    DescribeHook<GridViewColumnHeader>::Run(context);
    DescribeHook<GridViewColumn>::Run(context);
    DescribeHook<ViewBase>::Run(context);
    DescribeHook<GridView>::Run(context);
    DescribeHook<GridViewHeaderRowPresenter>::Run(context);
    DescribeHook<GridViewRowPresenter>::Run(context);
    DescribeHook<ListView>::Run(context);
    DescribeHook<ListViewItem>::Run(context);
    return {};
}

} // namespace Aero::Controls

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

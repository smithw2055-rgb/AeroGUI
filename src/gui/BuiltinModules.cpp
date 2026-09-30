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

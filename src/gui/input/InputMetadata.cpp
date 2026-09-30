// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

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

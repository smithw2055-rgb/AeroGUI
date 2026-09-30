// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>

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

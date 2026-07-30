#pragma once

#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Input {

using ICommand = ::Aero::Presentation::ICommand;
using InputGesture = ::Aero::Presentation::InputGesture;
using KeyGesture = ::Aero::Presentation::KeyGesture;
using RoutedCommand = ::Aero::Presentation::RoutedCommand;
using KeyBinding = ::Aero::Presentation::KeyBinding;
using CommandBinding = ::Aero::Presentation::CommandBinding;
using CanExecuteRoutedEventArgs =
    ::Aero::Presentation::CanExecuteRoutedEventArgs;
using ExecutedRoutedEventArgs =
    ::Aero::Presentation::ExecutedRoutedEventArgs;
using CanExecuteRoutedEventHandler =
    ::Aero::Presentation::CanExecuteRoutedEventHandler;
using ExecutedRoutedEventHandler =
    ::Aero::Presentation::ExecutedRoutedEventHandler;

using KeyboardNavigation =
    ::Aero::Presentation::KeyboardNavigation;
using KeyboardNavigationMode =
    ::Aero::Presentation::KeyboardNavigationMode;
using FocusNavigationDirection =
    ::Aero::Presentation::FocusNavigationDirection;

using MouseButton = ::Aero::Presentation::MouseButton;
using MouseButtonState =
    ::Aero::Presentation::MouseButtonState;
using KeyboardModifiers =
    ::Aero::Presentation::KeyboardModifiers;
using InputEventArgs = ::Aero::Presentation::InputEventArgs;
using MouseEventArgs = ::Aero::Presentation::MouseEventArgs;
using MouseButtonEventArgs =
    ::Aero::Presentation::MouseButtonEventArgs;
using MouseWheelEventArgs =
    ::Aero::Presentation::MouseWheelEventArgs;
using KeyEventArgs = ::Aero::Presentation::KeyEventArgs;
using TextCompositionEventArgs =
    ::Aero::Presentation::TextCompositionEventArgs;
using KeyboardFocusChangedEventArgs =
    ::Aero::Presentation::KeyboardFocusChangedEventArgs;

} // namespace Aero::Input

#include <Aero/Input/Keyboard.hpp>

#include "gui/internal/InputDevicesState.hpp"
#include "gui/input/InputState.hpp"

namespace Aero::Input {

const RoutedEventRef<Keyboard, KeyEventArgs> Keyboard::KeyDownEvent{
    "KeyDown"};
const RoutedEventRef<Keyboard, KeyEventArgs> Keyboard::PreviewKeyDownEvent{
    "PreviewKeyDown"};
const RoutedEventRef<Keyboard, KeyEventArgs> Keyboard::KeyUpEvent{"KeyUp"};
const RoutedEventRef<Keyboard, KeyEventArgs> Keyboard::PreviewKeyUpEvent{
    "PreviewKeyUp"};
const RoutedEventRef<Keyboard, KeyboardFocusChangedEventArgs>
    Keyboard::GotKeyboardFocusEvent{"GotKeyboardFocus"};
const RoutedEventRef<Keyboard, KeyboardFocusChangedEventArgs>
    Keyboard::LostKeyboardFocusEvent{"LostKeyboardFocus"};

::Aero::UIElement* Keyboard::FocusedElement() noexcept {
    InputRouter* router = DeviceState::ActiveRouter();
    return router != nullptr ? router->GetFocusedElement() : nullptr;
}

KeyboardModifiers Keyboard::Modifiers() noexcept {
    return static_cast<KeyboardModifiers>(DeviceState::LastModifiers());
}

} // namespace Aero::Input

#include <Aero/Input/Keyboard.hpp>

#include "gui/input/InputDevicesState.hpp"
#include "gui/core/Describe.hpp"

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

// Metadata registration for the types implemented in this file.
namespace Aero::Input {

AERO_DESCRIBE(Keyboard) {
    using namespace Aero::Meta;
    Register<Keyboard>(context, TypeFlags::Abstract)
            .Event(Keyboard::KeyDownEvent)
            .Event(Keyboard::PreviewKeyDownEvent)
            .Event(Keyboard::KeyUpEvent)
            .Event(Keyboard::PreviewKeyUpEvent)
            .Event(Keyboard::GotKeyboardFocusEvent)
            .Event(Keyboard::LostKeyboardFocusEvent);
}

} // namespace Aero::Input


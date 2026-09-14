#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Value.hpp>

namespace Aero::Input {

// Represents the keyboard device. Mirrors AeroUI's Aero::Keyboard: exposes the
// keyboard attached routed events and static accessors for focus/modifier state.
class AERO_GUI_API Keyboard : public Base::Object {
    AERO_DECLARE_TYPE(Keyboard, Base::Object)
public:
    static const RoutedEventRef<Keyboard, KeyEventArgs> KeyDownEvent;
    static const RoutedEventRef<Keyboard, KeyEventArgs> PreviewKeyDownEvent;
    static const RoutedEventRef<Keyboard, KeyEventArgs> KeyUpEvent;
    static const RoutedEventRef<Keyboard, KeyEventArgs> PreviewKeyUpEvent;
    static const RoutedEventRef<Keyboard, KeyboardFocusChangedEventArgs>
        GotKeyboardFocusEvent;
    static const RoutedEventRef<Keyboard, KeyboardFocusChangedEventArgs>
        LostKeyboardFocusEvent;

    // Element that currently has keyboard focus, or nullptr.
    static ::Aero::UIElement* FocusedElement() noexcept;

    // Currently pressed modifier keys.
    static KeyboardModifiers Modifiers() noexcept;

protected:
    Keyboard() = default;
};

} // namespace Aero::Input

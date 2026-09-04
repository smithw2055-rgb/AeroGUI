#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Value.hpp>

namespace Aero::Input {

// Represents the mouse device. Mirrors AeroUI's Aero::Mouse: exposes the set of
// mouse attached routed events and a handful of static accessors for the current
// pointer state. OS-level cursor application is performed by the view; this type
// only surfaces the logical state tracked by the input router.
//
// The attached events below are owned by Mouse (distinct RoutedEvent instances
// from the equivalent UIElement events) so they can be resolved via the
// {Mouse.MouseDown} XAML attached-event syntax.
class AERO_GUI_API Mouse : public Base::Object {
    AERO_DECLARE_TYPE(Mouse, Base::Object)
public:
    static const RoutedEventRef<Mouse, MouseButtonEventArgs> MouseDownEvent;
    static const RoutedEventRef<Mouse, MouseButtonEventArgs> PreviewMouseDownEvent;
    static const RoutedEventRef<Mouse, MouseButtonEventArgs> MouseUpEvent;
    static const RoutedEventRef<Mouse, MouseButtonEventArgs> PreviewMouseUpEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> MouseMoveEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> PreviewMouseMoveEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> MouseEnterEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> MouseLeaveEvent;
    static const RoutedEventRef<Mouse, MouseWheelEventArgs> MouseWheelEvent;
    static const RoutedEventRef<Mouse, MouseWheelEventArgs> PreviewMouseWheelEvent;

    // Mouse-capture lifecycle events (no direct UIElement equivalent).
    static const RoutedEventRef<Mouse, MouseEventArgs> GotMouseCaptureEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> LostMouseCaptureEvent;
    static const RoutedEventRef<Mouse, MouseEventArgs> QueryCursorEvent;

    // Returns the mouse position in root (client) coordinates, or relative to
    // the supplied element when one is provided.
    static Base::Point GetPosition(::Aero::UIElement* relativeTo);

    // Element that currently has mouse capture, or nullptr.
    static ::Aero::UIElement* Captured() noexcept;

    // Application-level override cursor, or nullptr when not overridden.
    static Base::Ref<Cursor> OverrideCursor() noexcept;
    static void SetOverrideCursor(const Base::Ref<Cursor>& cursor) noexcept;
    static void SetOverrideCursor(std::nullptr_t) noexcept;

protected:
    Mouse() = default;
};

} // namespace Aero::Input

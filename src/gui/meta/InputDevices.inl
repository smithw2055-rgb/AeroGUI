// Mouse/Keyboard input-device type registration. Kept separate so the heavy
// UIElement-dependent headers stay out of the anon-namespaced populate units.
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>

namespace Aero {

Base::Result<void> PopulateInputDevices(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    using namespace Aero::Input;

    Register<Mouse>(context, TypeFlags::Abstract)
        .Event(Mouse::MouseDownEvent)
        .Event(Mouse::PreviewMouseDownEvent)
        .Event(Mouse::MouseUpEvent)
        .Event(Mouse::PreviewMouseUpEvent)
        .Event(Mouse::MouseMoveEvent)
        .Event(Mouse::PreviewMouseMoveEvent)
        .Event(Mouse::MouseEnterEvent)
        .Event(Mouse::MouseLeaveEvent)
        .Event(Mouse::MouseWheelEvent)
        .Event(Mouse::PreviewMouseWheelEvent)
        .Event(Mouse::GotMouseCaptureEvent)
        .Event(Mouse::LostMouseCaptureEvent)
        .Event(Mouse::QueryCursorEvent);

    Register<Keyboard>(context, TypeFlags::Abstract)
        .Event(Keyboard::KeyDownEvent)
        .Event(Keyboard::PreviewKeyDownEvent)
        .Event(Keyboard::KeyUpEvent)
        .Event(Keyboard::PreviewKeyUpEvent)
        .Event(Keyboard::GotKeyboardFocusEvent)
        .Event(Keyboard::LostKeyboardFocusEvent);

    Register<DataObject>(context);

    Register<DragDrop>(context, TypeFlags::Abstract)
        .Event(DragDrop::PreviewDragEnterEvent)
        .Event(DragDrop::DragEnterEvent)
        .Event(DragDrop::PreviewDragOverEvent)
        .Event(DragDrop::DragOverEvent)
        .Event(DragDrop::PreviewDragLeaveEvent)
        .Event(DragDrop::DragLeaveEvent)
        .Event(DragDrop::PreviewDropEvent)
        .Event(DragDrop::DropEvent);

    return {};
}

} // namespace Aero

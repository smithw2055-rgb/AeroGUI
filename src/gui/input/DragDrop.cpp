#include <Aero/DragDrop.hpp>

#include <Aero/DependencyObject.hpp>
#include <Aero/Meta.hpp>
#include <Aero/TryCast.hpp>

#include "gui/input/InputState.hpp"
#include "gui/internal/InputDevicesState.hpp"

namespace Aero {

const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::PreviewDragEnterEvent{
    "PreviewDragEnter"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::DragEnterEvent{"DragEnter"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::PreviewDragOverEvent{
    "PreviewDragOver"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::DragOverEvent{"DragOver"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::PreviewDragLeaveEvent{
    "PreviewDragLeave"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::DragLeaveEvent{"DragLeave"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::PreviewDropEvent{
    "PreviewDrop"};
const RoutedEventRef<DragDrop, DragEventArgs> DragDrop::DropEvent{"Drop"};

Input::DragDropEffects DragDrop::DoDragDrop(
    ::Aero::DependencyObject* source,
    const Base::Ref<Base::Object>& data,
    Input::DragDropEffects allowedEffects) noexcept {
    if (source == nullptr) {
        return Input::DragDropEffects::None;
    }
    InputRouter* router = Input::DeviceState::ActiveRouter();
    UIElement* element = TryCast<UIElement>(source);
    if (router == nullptr || element == nullptr) {
        return Input::DragDropEffects::None;
    }
    Meta::Value payload = !data
        ? Meta::Value{}
        : Meta::Value::FromObject(Meta::TypeOf<Base::Object>(), data);
    const Base::Result<void> begun = router->BeginDrag(*element, 0U, payload, allowedEffects);
    if (!begun) {
        return Input::DragDropEffects::None;
    }
    return allowedEffects;
}

} // namespace Aero

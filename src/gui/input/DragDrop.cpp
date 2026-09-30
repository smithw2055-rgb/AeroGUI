#include <Aero/DragDrop.hpp>

#include <Aero/DependencyObject.hpp>
#include <Aero/Meta.hpp>
#include <Aero/TryCast.hpp>

#include "gui/input/InputManager.hpp"
#include "gui/input/InputDevicesState.hpp"
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
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

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

// Metadata registration for the types implemented in this file.
namespace Aero {

AERO_DESCRIBE(DragDrop) {
    using namespace Aero::Meta;
    Register<DragDrop>(context, TypeFlags::Abstract)
            .Event(DragDrop::PreviewDragEnterEvent)
            .Event(DragDrop::DragEnterEvent)
            .Event(DragDrop::PreviewDragOverEvent)
            .Event(DragDrop::DragOverEvent)
            .Event(DragDrop::PreviewDragLeaveEvent)
            .Event(DragDrop::DragLeaveEvent)
            .Event(DragDrop::PreviewDropEvent)
            .Event(DragDrop::DropEvent);
}

} // namespace Aero


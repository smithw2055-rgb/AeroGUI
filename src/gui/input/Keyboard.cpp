#include <Aero/Input/Keyboard.hpp>

#include "gui/input/InputDevicesState.hpp"
#include "gui/input/InputManager.hpp"
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
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

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
AERO_DESCRIBE(::Aero::Input::Keyboard) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Input;
    using namespace Aero::Meta;
    using namespace Aero::Input;
    Register<Keyboard>(context, TypeFlags::Abstract)
            .Event(Keyboard::KeyDownEvent)
            .Event(Keyboard::PreviewKeyDownEvent)
            .Event(Keyboard::KeyUpEvent)
            .Event(Keyboard::PreviewKeyUpEvent)
            .Event(Keyboard::GotKeyboardFocusEvent)
            .Event(Keyboard::LostKeyboardFocusEvent);
}

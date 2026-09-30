#include <Aero/Input/Mouse.hpp>

#include <Aero/Visual.hpp>

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
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Input {

const RoutedEventRef<Mouse, MouseButtonEventArgs> Mouse::MouseDownEvent{
    "MouseDown"};
const RoutedEventRef<Mouse, MouseButtonEventArgs> Mouse::PreviewMouseDownEvent{
    "PreviewMouseDown"};
const RoutedEventRef<Mouse, MouseButtonEventArgs> Mouse::MouseUpEvent{
    "MouseUp"};
const RoutedEventRef<Mouse, MouseButtonEventArgs> Mouse::PreviewMouseUpEvent{
    "PreviewMouseUp"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::MouseMoveEvent{
    "MouseMove"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::PreviewMouseMoveEvent{
    "PreviewMouseMove"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::MouseEnterEvent{
    "MouseEnter"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::MouseLeaveEvent{
    "MouseLeave"};
const RoutedEventRef<Mouse, MouseWheelEventArgs> Mouse::MouseWheelEvent{
    "MouseWheel"};
const RoutedEventRef<Mouse, MouseWheelEventArgs> Mouse::PreviewMouseWheelEvent{
    "PreviewMouseWheel"};

const RoutedEventRef<Mouse, MouseEventArgs> Mouse::GotMouseCaptureEvent{
    "GotMouseCapture"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::LostMouseCaptureEvent{
    "LostMouseCapture"};
const RoutedEventRef<Mouse, MouseEventArgs> Mouse::QueryCursorEvent{
    "QueryCursor"};

Base::Point Mouse::GetPosition(::Aero::UIElement* relativeTo) {
    InputRouter* router = DeviceState::ActiveRouter();
    Base::Point position = DeviceState::LastPointerPosition();
    if (relativeTo != nullptr && router != nullptr) {
        ElementTree* tree = VisualTree(relativeTo);
        if (tree != nullptr) {
            ::Aero::Media::Visual* root = tree->Root();
            if (root != nullptr) {
                Base::Result<Input::HitTestResult> local =
                    router->RootToLocal(*root, *relativeTo, position);
                if (local) position = local.Value().position;
            }
        }
    }
    return position;
}

::Aero::UIElement* Mouse::Captured() noexcept {
    InputRouter* router = DeviceState::ActiveRouter();
    return router != nullptr ? router->GetCapturedPointer(0U) : nullptr;
}

Base::Ref<Cursor> Mouse::OverrideCursor() noexcept {
    return DeviceState::OverrideCursor();
}

void Mouse::SetOverrideCursor(const Base::Ref<Cursor>& cursor) noexcept {
    DeviceState::SetOverrideCursor(cursor);
}

void Mouse::SetOverrideCursor(std::nullptr_t) noexcept {
    DeviceState::ClearOverrideCursor();
}

} // namespace Aero::Input

namespace Aero::Input::DeviceState {

static InputRouter* g_activeRouter = nullptr;
static Base::Point g_lastPointerPosition{};
static std::uint32_t g_lastModifiers = 0U;
static Base::Ref<Cursor> g_overrideCursor;

InputRouter* ActiveRouter() noexcept { return g_activeRouter; }
void SetActiveRouter(InputRouter* router) noexcept {
    g_activeRouter = router;
}

Base::Point LastPointerPosition() noexcept { return g_lastPointerPosition; }
void SetLastPointerPosition(const Base::Point& position) noexcept {
    g_lastPointerPosition = position;
}

std::uint32_t LastModifiers() noexcept { return g_lastModifiers; }
void SetLastModifiers(std::uint32_t modifiers) noexcept {
    g_lastModifiers = modifiers;
}

Base::Ref<Cursor> OverrideCursor() noexcept { return g_overrideCursor; }
void SetOverrideCursor(const Base::Ref<Cursor>& cursor) noexcept {
    g_overrideCursor = cursor;
}
void ClearOverrideCursor() noexcept { g_overrideCursor = Base::Ref<Cursor>{}; }

} // namespace Aero::Input::DeviceState

// Metadata registration for the types implemented in this file.
namespace Aero::Input {

AERO_DESCRIBE(Mouse) {
    using namespace Aero::Meta;
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
}

} // namespace Aero::Input


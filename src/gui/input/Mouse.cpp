#include <Aero/Input/Mouse.hpp>

#include <Aero/Visual.hpp>

#include "gui/internal/InputDevicesState.hpp"
#include "gui/input/InputState.hpp"

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
        ElementTree* tree = relativeTo->GetTree();
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

#include "gui/ViewFrame.hpp"
#include "gui/input/InputDevicesState.hpp"
#include <Aero/Base/Utf8.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

namespace {

Base::Result<Input::PointerDispatchResult> DispatchPointer(
    ViewFrame& state,
    const Input::PointerInput& input) noexcept {
    if (!state.mounted || state.Input() == nullptr) {
        return ViewNotInitialized(
            "Pointer input requires a mounted View");
    }
    Input::DeviceState::SetActiveRouter(state.Input());
    Input::DeviceState::SetLastPointerPosition(input.position);
    // Close popups on an outside press *before* routing, then hit-test
    // again. Otherwise a stale open Popup overlay keeps capturing the
    // sidebar (and ComboBox toggles reopen the theme dropdown).
    if (state.Overlays() != nullptr &&
        input.action == Input::PointerAction::Down) {
        Aero::UIElement* preTarget = nullptr;
        Aero::Media::Visual* root = state.RootVisual();
        if (root != nullptr) {
            Base::Result<Input::HitTestResult> preHit =
                state.Input()->HitTest(*root, input.position);
            if (!preHit) {
                return preHit.GetStatus();
            }
            preTarget = preHit.Value().target;
        }
        Base::Result<void> dismissed =
            state.Overlays()->DismissOverlaysForPointer(
                input, preTarget);
        if (!dismissed) {
            return dismissed.GetStatus();
        }
        Base::Result<void> synced =
            state.Overlays()->SynchronizeOverlays();
        if (!synced) {
            return synced.GetStatus();
        }
    }
    Base::Result<
        Input::PointerDispatchResult>
        dispatched =
            state.Input()->DispatchPointer(input);
    if (!dispatched) {
        return dispatched.GetStatus();
    }
    Aero::UIElement* target =
        dispatched.Value().hit.target;
    if (state.Overlays() == nullptr) {
        return dispatched;
    }
    state.Overlays()->UpdateToolTipForPointer(input, target);
    state.Overlays()->OpenContextMenuForPointer(input, target);
    return dispatched;
}

Base::Result<Input::KeyboardDispatchResult>
DispatchKeyboard(
    ViewFrame& state,
    const Input::KeyboardInput& input) noexcept {
    if (!state.mounted || state.Input() == nullptr) {
        return ViewNotInitialized(
            "Keyboard input requires a mounted View");
    }
    Input::DeviceState::SetActiveRouter(state.Input());
    Input::DeviceState::SetLastModifiers(input.modifiers);
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape &&
        state.Input()->IsDragging()) {
        return state.Input()->DispatchKeyboard(input);
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape) {
        if (state.Overlays() != nullptr &&
            state.Overlays()->DismissTopOverlayForEscape()) {
            Input::KeyboardDispatchResult
                result;
            result.routed = true;
            return result;
        }
    }
    return state.Input()->DispatchKeyboard(input);
}

Base::Result<Input::TextInputDispatchResult>
DispatchText(
    ViewFrame& state,
    const Input::TextInput& input) noexcept {
    if (!state.mounted || state.Input() == nullptr) {
        return ViewNotInitialized(
            "Text input requires a mounted View");
    }
    return state.Input()->DispatchText(input);
}

bool DispatchTouch(
    ViewFrame* state,
    Input::PointerAction action,
    int x,
    int y,
    std::uint64_t id) noexcept {
    if (id >= static_cast<std::uint64_t>(UINT32_MAX)) return false;
    Input::PointerInput input;
    input.pointerId = static_cast<std::uint32_t>(id) + 1U;
    input.action = action;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    if (state == nullptr) return false;
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state, input);
    return dispatched && dispatched.Value().routed;
}

} // namespace

bool View::MouseMove(int x, int y) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Move;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonDown(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonUp(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Up;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::MouseDoubleClick(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    input.clickCount = 2U;
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::MouseWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaY = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::MouseHWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_ || state_ == nullptr) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaX = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::KeyDown(Input::Key key) noexcept {
    if (!active_ || state_ == nullptr) return false;
    std::uint32_t currentModifiers = Input::DeviceState::LastModifiers();
    const std::uint32_t rawKey = static_cast<std::uint32_t>(key);
    if (key == Input::Key::Shift || key == Input::Key::LeftShift || key == Input::Key::RightShift) {
        currentModifiers |= static_cast<std::uint32_t>(Input::KeyboardModifiers::Shift);
    } else if (key == Input::Key::Control || key == Input::Key::LeftCtrl || key == Input::Key::RightCtrl) {
        currentModifiers |= static_cast<std::uint32_t>(Input::KeyboardModifiers::Control);
    } else if (key == Input::Key::Alt || key == Input::Key::LeftAlt || key == Input::Key::RightAlt) {
        currentModifiers |= static_cast<std::uint32_t>(Input::KeyboardModifiers::Alt);
    }
    Input::DeviceState::SetLastModifiers(currentModifiers);

    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Down;
    input.key = rawKey;
    input.modifiers = currentModifiers;
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        DispatchKeyboard(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::KeyUp(Input::Key key) noexcept {
    if (!active_ || state_ == nullptr) return false;
    std::uint32_t currentModifiers = Input::DeviceState::LastModifiers();
    const std::uint32_t rawKey = static_cast<std::uint32_t>(key);
    if (key == Input::Key::Shift || key == Input::Key::LeftShift || key == Input::Key::RightShift) {
        currentModifiers &= ~static_cast<std::uint32_t>(Input::KeyboardModifiers::Shift);
    } else if (key == Input::Key::Control || key == Input::Key::LeftCtrl || key == Input::Key::RightCtrl) {
        currentModifiers &= ~static_cast<std::uint32_t>(Input::KeyboardModifiers::Control);
    } else if (key == Input::Key::Alt || key == Input::Key::LeftAlt || key == Input::Key::RightAlt) {
        currentModifiers &= ~static_cast<std::uint32_t>(Input::KeyboardModifiers::Alt);
    }
    Input::DeviceState::SetLastModifiers(currentModifiers);

    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Up;
    input.key = rawKey;
    input.modifiers = currentModifiers;
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        DispatchKeyboard(*state_, input);
    return dispatched && dispatched.Value().routed;
}

bool View::Char(std::uint32_t codePoint) noexcept {
    if (!active_ || state_ == nullptr || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return false;
    }
    char text[4]{};
    const std::uint32_t length = Base::EncodeCodePoint(codePoint, text);
    if (length == 0U) {
        return false;
    }
    Base::Result<Input::TextInputDispatchResult> dispatched =
        DispatchText(
            *state_, {Base::StringView(text, length)});
    return dispatched && dispatched.Value().routed;
}

bool View::TouchDown(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && state_ != nullptr && DispatchTouch(
        state_, Input::PointerAction::Down, x, y, id);
}

bool View::TouchMove(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && state_ != nullptr && DispatchTouch(
        state_, Input::PointerAction::Move, x, y, id);
}

bool View::TouchUp(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && state_ != nullptr && DispatchTouch(
        state_, Input::PointerAction::Up, x, y, id);
}
} // namespace Aero

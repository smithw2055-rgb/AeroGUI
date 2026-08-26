#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

namespace {

Base::Result<Input::PointerDispatchResult> DispatchPointer(
    ViewState& state,
    const Input::PointerInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Pointer input requires a mounted View");
    }
    Base::Result<
        Input::PointerDispatchResult>
        dispatched =
            state.input->DispatchPointer(input);
    if (!dispatched) {
        return dispatched.GetStatus();
    }
    Aero::UIElement* target =
        dispatched.Value().hit.target;
    if (state.overlays == nullptr) {
        return dispatched;
    }
    Base::Result<void> dismissed =
        state.overlays->DismissOverlaysForPointer(
            input, target);
    if (!dismissed) {
        return dismissed.GetStatus();
    }
    Base::Result<void> toolTip =
        state.overlays->UpdateToolTipForPointer(
            input, target);
    if (!toolTip) {
        return toolTip.GetStatus();
    }
    Base::Result<void> contextMenu =
        state.overlays->OpenContextMenuForPointer(
            input, target);
    if (!contextMenu) {
        return contextMenu.GetStatus();
    }
    return dispatched;
}

Base::Result<Input::KeyboardDispatchResult>
DispatchKeyboard(
    ViewState& state,
    const Input::KeyboardInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Keyboard input requires a mounted View");
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape &&
        state.input->IsDragging()) {
        return state.input->DispatchKeyboard(input);
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape) {
        Base::Result<bool> dismissed =
            state.overlays != nullptr
                ? state.overlays->DismissTopOverlayForEscape()
                : Base::Result<bool>(false);
        if (!dismissed) {
            return dismissed.GetStatus();
        }
        if (dismissed.Value()) {
            Input::KeyboardDispatchResult
                result;
            result.routed = true;
            return result;
        }
    }
    return state.input->DispatchKeyboard(input);
}

Base::Result<Input::TextInputDispatchResult>
DispatchText(
    ViewState& state,
    const Input::TextInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Text input requires a mounted View");
    }
    return state.input->DispatchText(input);
}

bool DispatchTouch(
    ViewState* state,
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
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Move;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonDown(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonUp(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Up;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseDoubleClick(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    input.clickCount = 2U;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaY = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseHWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaX = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::KeyDown(Input::Key key) noexcept {
    if (!active_) return false;
    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Down;
    input.key = static_cast<std::uint32_t>(key);
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchKeyboard(*state_, input)
        : Base::Result<Input::KeyboardDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::KeyUp(Input::Key key) noexcept {
    if (!active_) return false;
    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Up;
    input.key = static_cast<std::uint32_t>(key);
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchKeyboard(*state_, input)
        : Base::Result<Input::KeyboardDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::Char(std::uint32_t codePoint) noexcept {
    if (!active_ || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return false;
    }
    char text[4]{};
    std::uint32_t length = 0U;
    if (codePoint <= 0x7FU) {
        text[length++] = static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FFU) {
        text[length++] = static_cast<char>(0xC0U | (codePoint >> 6U));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else if (codePoint <= 0xFFFFU) {
        text[length++] = static_cast<char>(0xE0U | (codePoint >> 12U));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 6U) & 0x3FU));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else {
        text[length++] = static_cast<char>(0xF0U | (codePoint >> 18U));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 12U) & 0x3FU));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 6U) & 0x3FU));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    }
    Base::Result<Input::TextInputDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchText(
              *state_, {Base::StringView(text, length)})
        : Base::Result<Input::TextInputDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::TouchDown(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Down, x, y, id);
}

bool View::TouchMove(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Move, x, y, id);
}

bool View::TouchUp(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Up, x, y, id);
}


} // namespace Aero

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
    Base::Result<void> toolTip =
        state.Overlays()->UpdateToolTipForPointer(
            input, target);
    if (!toolTip) {
        return toolTip.GetStatus();
    }
    Base::Result<void> contextMenu =
        state.Overlays()->OpenContextMenuForPointer(
            input, target);
    if (!contextMenu) {
        return contextMenu.GetStatus();
    }
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
        Base::Result<bool> dismissed =
            state.Overlays() != nullptr
                ? state.Overlays()->DismissTopOverlayForEscape()
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
    if (rawKey == 0x10U || rawKey == 0xA0U || rawKey == 0xA1U) {
        currentModifiers |= static_cast<std::uint32_t>(Input::KeyboardModifiers::Shift);
    } else if (rawKey == 0x11U || rawKey == 0xA2U || rawKey == 0xA3U) {
        currentModifiers |= static_cast<std::uint32_t>(Input::KeyboardModifiers::Control);
    } else if (rawKey == 0x12U || rawKey == 0xA4U || rawKey == 0xA5U) {
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
    if (rawKey == 0x10U || rawKey == 0xA0U || rawKey == 0xA1U) {
        currentModifiers &= ~static_cast<std::uint32_t>(Input::KeyboardModifiers::Shift);
    } else if (rawKey == 0x11U || rawKey == 0xA2U || rawKey == 0xA3U) {
        currentModifiers &= ~static_cast<std::uint32_t>(Input::KeyboardModifiers::Control);
    } else if (rawKey == 0x12U || rawKey == 0xA4U || rawKey == 0xA5U) {
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


// ---- Section: Focus queue (merged from ViewFocus.cpp) ----

FocusHost::FocusHost(ViewFrame& owner) noexcept
    : view(&owner),
      pendingFocusTargets(owner.allocator) {}

void FocusHost::Bind() noexcept {
    // Input is read on demand from the ElementTree hub via Input().
}

Aero::InputManager* FocusHost::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

Base::Result<void> FocusHost::QueueFocus(Aero::UIElement& target) noexcept {
        Base::Ref<Aero::UIElement> retained =
            Base::Ref<Aero::UIElement>::FromBorrowed(target);
        for (const Base::WeakRef<Aero::UIElement>& pending :
             pendingFocusTargets) {
            Base::Ref<Aero::UIElement> existing = pending.Lock();
            if (existing.Get() == &target) return {};
        }
        pendingFocusTargets.PushBack(
            Base::WeakRef<Aero::UIElement>(retained));
        return {};
    }

Base::Result<std::uint32_t> FocusHost::ProcessPendingFocus() noexcept {
        Aero::InputManager* input = Input();
        if (input == nullptr || pendingFocusTargets.Empty()) return 0U;
        std::uint32_t focusedCount = 0U;
        std::uint32_t output = 0U;
        for (std::uint32_t index = 0U;
             index < pendingFocusTargets.Size(); ++index) {
            Base::Ref<Aero::UIElement> target =
                pendingFocusTargets[index].Lock();
            if (!target) continue;
            if (!target->GetIsLoaded()) {
                if (output != index) {
                    pendingFocusTargets[output] =
                        std::move(pendingFocusTargets[index]);
                }
                ++output;
                continue;
            }
            if (!target->GetIsEnabled()) continue;
            Base::Result<bool> focused = input->SetFocus(target.Get());
            if (!focused) return focused.GetStatus();
            if (focused.Value()) ++focusedCount;
        }
        pendingFocusTargets.Resize(output);
        return focusedCount;
    }


} // namespace Aero

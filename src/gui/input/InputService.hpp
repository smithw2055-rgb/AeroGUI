#pragma once

#include "gui/input/InputState.hpp"

namespace Aero::Detail {

// View-owned input coordinator. Consumers see one service; focus, hit testing,
// pointer capture, keyboard/text dispatch and routed commands remain private
// implementation components behind this facade.
class InputService final {
public:
    InputService(ObjectTree& tree, EventRouter& events) noexcept
        : commands_(tree, events),
          focus_(tree, events),
          pointer_(hitTests_, events),
          keyboard_(focus_, events, tree, &commands_),
          text_(focus_, events, tree) {}

    void SetRoot(Visual* root) noexcept { pointer_.SetRoot(root); }

    Base::Result<PointerDispatchResult> DispatchPointer(const PointerInput& input) noexcept { return pointer_.Dispatch(input); }
    Base::Result<KeyboardDispatchResult> DispatchKeyboard(const KeyboardInput& input) noexcept { return keyboard_.Dispatch(input); }
    Base::Result<TextInputDispatchResult> DispatchText(const TextInput& input) noexcept { return text_.Dispatch(input); }

    Base::Result<void> SetOverlays(Base::Span<UIElement* const> overlays, Base::Span<const Point> origins) noexcept { return hitTests_.SetOverlays(overlays, origins); }
    void ClearOverlays() noexcept { hitTests_.ClearOverlays(); }
    Base::Result<HitTestResult> HitTest(Visual& root, Point position) const noexcept { return hitTests_.HitTest(root, position); }
    Base::Result<HitTestResult> RootToLocal(Visual& root, Visual& target, Point position) const noexcept { return hitTests_.RootToLocal(root, target, position); }

    Base::Result<void> CapturePointer(std::uint32_t pointerId, UIElement& target) noexcept { return pointer_.CapturePointer(pointerId, target); }
    Base::Result<bool> ReleasePointer(std::uint32_t pointerId) noexcept { return pointer_.ReleasePointer(pointerId); }
    UIElement* GetCapturedPointer(std::uint32_t pointerId) noexcept { return pointer_.CapturedNode(pointerId); }
    Base::Result<void> TryAddPointerStateChanged(const PointerStateChangedHandler& handler) noexcept { return pointer_.TryAddStateChanged(handler); }
    bool RemovePointerStateChanged(const PointerStateChangedHandler& handler) noexcept { return pointer_.RemoveStateChanged(handler); }
    Base::Result<void> TryAddPointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { return pointer_.TryAddCaptureChanged(handler); }
    bool RemovePointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { return pointer_.RemoveCaptureChanged(handler); }

    UIElement* GetFocusedElement() noexcept { return focus_.FocusedNode(); }
    UIElement* GetFocusedElement(UIElement& scope) noexcept { return focus_.FocusedElement(scope); }
    Base::Result<bool> SetFocus(UIElement* element) noexcept { return focus_.SetFocus(element); }
    Base::Result<bool> ClearFocus() noexcept { return focus_.ClearFocus(); }
    Base::Result<bool> MoveFocus(FocusNavigationDirection direction, bool wrap = true) noexcept { return focus_.MoveFocus(direction, wrap); }

    Base::Result<CommandBindingHandle> TryAddCommandBinding(UIElement& owner, const CommandBinding& binding) noexcept { return commands_.TryAddBinding(owner, binding); }
    Base::Result<bool> RemoveCommandBinding(CommandBindingHandle handle) noexcept { return commands_.RemoveBinding(handle); }
    Base::Result<InputBindingHandle> TryAddInputBinding(UIElement& owner, Base::Ref<KeyBinding> binding) noexcept { return commands_.TryAddInputBinding(owner, std::move(binding)); }
    Base::Result<void> TryAddRequerySuggested(const RequerySuggestedHandler& handler) noexcept { return commands_.TryAddRequerySuggested(handler); }
    bool RemoveRequerySuggested(const RequerySuggestedHandler& handler) noexcept { return commands_.RemoveRequerySuggested(handler); }
    void InvalidateRequerySuggested() const noexcept { commands_.InvalidateRequerySuggested(); }

    Base::Result<bool> CanExecute(ICommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> CanExecute(RoutedCommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> Execute(ICommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }
    Base::Result<bool> Execute(RoutedCommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }

private:
    CommandState commands_;
    HitTestState hitTests_;
    FocusState focus_;
    PointerStateMachine pointer_;
    KeyboardState keyboard_;
    TextInputState text_;
};

} // namespace Aero::Detail

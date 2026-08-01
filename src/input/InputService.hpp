#pragma once

#include "../ui/RuntimeManagers.hpp"

namespace Aero::Detail {

// View-owned input coordinator. It is the single runtime attachment exposed to
// UIElement; the focused element, pointer capture, keyboard/text dispatch and
// routed commands all share the same ObjectTree and EventRouter.
class InputService final {
public:
    InputService(ObjectTree& tree, EventRouter& events) noexcept
        : commands_(tree),
          focus_(tree, events),
          pointer_(hitTests_, events),
          keyboard_(focus_, events, tree, &commands_),
          text_(focus_, events, tree) {}

    void SetRoot(Visual* root) noexcept {
        pointer_.SetRoot(root);
    }

    CommandManager& Commands() noexcept { return commands_; }
    HitTestManager& HitTests() noexcept { return hitTests_; }
    FocusManager& Focus() noexcept { return focus_; }
    PointerInputManager& Pointer() noexcept { return pointer_; }
    KeyboardInputManager& Keyboard() noexcept { return keyboard_; }
    TextInputManager& Text() noexcept { return text_; }

    Base::Result<bool> CanExecute(
        Input::RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept {
        return commands_.CanExecute(command, parameter, target);
    }

    Base::Result<bool> Execute(
        Input::RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept {
        return commands_.Execute(command, parameter, target);
    }

private:
    CommandManager commands_;
    HitTestManager hitTests_;
    FocusManager focus_;
    PointerInputManager pointer_;
    KeyboardInputManager keyboard_;
    TextInputManager text_;
};

} // namespace Aero::Detail

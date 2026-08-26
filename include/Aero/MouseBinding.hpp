#pragma once

#include <Aero/InputBinding.hpp>
#include <Aero/Input.hpp>

namespace Aero::Input {

class AERO_GUI_API MouseBinding : public InputBinding {
    AERO_DECLARE_TYPE(MouseBinding, InputBinding)
public:
    MouseBinding() noexcept : InputBinding(StaticTypeId()) {}

    MouseButton GetMouseButton() const noexcept { return button_; }
    void SetMouseButton(MouseButton value) noexcept { button_ = value; }
    PointerAction GetAction() const noexcept { return action_; }
    void SetAction(PointerAction value) noexcept { action_ = value; }
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    void SetCommandName(StringView value) noexcept;

    bool Matches(const PointerInput& input) const noexcept;
    Result<void> Finalize() noexcept override;

private:
    MouseButton button_ = MouseButton::Left;
    PointerAction action_ = PointerAction::Down;
    String commandName_;
};

} // namespace Aero::Input

#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

class AERO_GUI_API InputBinding : public Base::Object {
    AERO_DECLARE_TYPE(InputBinding, Base::Object)

public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommand(Ref<RoutedCommand> value) noexcept { command_ = std::move(value); }
    virtual bool Finalize() noexcept { return true; }

protected:
    explicit InputBinding(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

    Ref<RoutedCommand> command_;

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
};

class AERO_GUI_API KeyBinding : public InputBinding {
    AERO_DECLARE_TYPE(KeyBinding, InputBinding)

public:
    KeyBinding() noexcept : InputBinding(StaticTypeId()) {}
    ~KeyBinding() override = default;

    StringView GetCommandName() const noexcept { return commandName_.View(); }
    StringView GetKeyName() const noexcept { return keyName_.View(); }
    StringView GetModifiersName() const noexcept { return modifiersName_.View(); }
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    bool Finalize() noexcept override;
    void ReleaseAuthored() noexcept {
        commandName_.Clear();
        keyName_.Clear();
        modifiersName_.Clear();
    }

private:
    String commandName_;
    String keyName_;
    String modifiersName_;
    bool finalized_ = false;
};

class AERO_GUI_API MouseBinding : public InputBinding {
    AERO_DECLARE_TYPE(MouseBinding, InputBinding)

public:
    MouseBinding() noexcept : InputBinding(StaticTypeId()) {}
    ~MouseBinding() override = default;

    MouseButton GetMouseButton() const noexcept { return button_; }
    void SetMouseButton(MouseButton value) noexcept { button_ = value; }
    PointerAction GetAction() const noexcept { return action_; }
    void SetAction(PointerAction value) noexcept { action_ = value; }
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    void SetCommandName(StringView value) noexcept;

    bool Matches(const PointerInput& input) const noexcept;
    bool Finalize() noexcept override;
    void ReleaseAuthored() noexcept {
        commandName_.Clear();
    }

private:
    MouseButton button_ = MouseButton::Left;
    PointerAction action_ = PointerAction::Down;
    String commandName_;
    bool finalized_ = false;
};

} // namespace Aero::Input

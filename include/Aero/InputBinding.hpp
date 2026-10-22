#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

class AERO_GUI_API InputBinding : public Base::Object {
    AERO_DECLARE_TYPE(InputBinding, Base::Object)

public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommand(Ref<RoutedCommand> value) noexcept { command_ = std::move(value); }
    virtual Result<void> Finalize() noexcept { return {}; }

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
    ~KeyBinding() override;

    StringView GetCommandName() const noexcept;
    StringView GetKeyName() const noexcept;
    StringView GetModifiersName() const noexcept;
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    Result<void> Finalize() noexcept override;
    void ReleaseAuthored() noexcept;

private:
    struct AuthoredData {
        AuthoredData() noexcept {}
        String commandName;
        String keyName;
        String modifiersName;
    };
    AuthoredData* EnsureAuthored() noexcept;

    AuthoredData* authored_ = nullptr;
    bool finalized_ = false;
};

class AERO_GUI_API MouseBinding : public InputBinding {
    AERO_DECLARE_TYPE(MouseBinding, InputBinding)

public:
    MouseBinding() noexcept : InputBinding(StaticTypeId()) {}
    ~MouseBinding() override;

    MouseButton GetMouseButton() const noexcept { return button_; }
    void SetMouseButton(MouseButton value) noexcept { button_ = value; }
    PointerAction GetAction() const noexcept { return action_; }
    void SetAction(PointerAction value) noexcept { action_ = value; }
    StringView GetCommandName() const noexcept;
    void SetCommandName(StringView value) noexcept;

    bool Matches(const PointerInput& input) const noexcept;
    Result<void> Finalize() noexcept override;
    void ReleaseAuthored() noexcept;

private:
    struct AuthoredData {
        AuthoredData() noexcept {}
        String commandName;
    };
    AuthoredData* EnsureAuthored() noexcept;

    MouseButton button_ = MouseButton::Left;
    PointerAction action_ = PointerAction::Down;
    AuthoredData* authored_ = nullptr;
    bool finalized_ = false;
};

} // namespace Aero::Input

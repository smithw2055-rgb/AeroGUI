#pragma once

#include <Aero/RoutedCommand.hpp>
#include <new>

namespace Aero::Input {

class AERO_GUI_API InputBinding : public Base::Object {
    AERO_DECLARE_TYPE(InputBinding, Base::Object)

public:
    // Inherits Object::RuntimeType() via GetClassType()
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommand(Ref<RoutedCommand> value) noexcept { command_ = std::move(value); }
    virtual bool Finalize() noexcept { return true; }

protected:
    InputBinding() noexcept = default;
    explicit InputBinding(Meta::TypeId runtimeType) noexcept { static_cast<void>(runtimeType); }

    Ref<RoutedCommand> command_;

private:
    // runtimeType_ removed; identity via StaticClassType()
};

class AERO_GUI_API KeyBinding : public InputBinding {
    AERO_DECLARE_TYPE(KeyBinding, InputBinding)

public:
    KeyBinding() noexcept : InputBinding(StaticTypeId()) {}
    ~KeyBinding() override { delete authored_; }

    StringView GetCommandName() const noexcept {
        return authored_ != nullptr ? authored_->commandName.View() : StringView{};
    }
    StringView GetKeyName() const noexcept {
        return authored_ != nullptr ? authored_->keyName.View() : StringView{};
    }
    StringView GetModifiersName() const noexcept {
        return authored_ != nullptr ? authored_->modifiersName.View() : StringView{};
    }
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    bool Finalize() noexcept override;
    void ReleaseAuthored() noexcept {
        delete authored_;
        authored_ = nullptr;
    }

private:
    struct AuthoredData {
        String commandName{&Base::GetDefaultAllocator()};
        String keyName{&Base::GetDefaultAllocator()};
        String modifiersName{&Base::GetDefaultAllocator()};
    };

    AuthoredData* EnsureAuthored() noexcept {
        if (authored_ == nullptr) {
            authored_ = new (std::nothrow) AuthoredData();
        }
        return authored_;
    }

    AuthoredData* authored_ = nullptr;
    bool finalized_ = false;
};

class AERO_GUI_API MouseBinding : public InputBinding {
    AERO_DECLARE_TYPE(MouseBinding, InputBinding)

public:
    MouseBinding() noexcept : InputBinding(StaticTypeId()) {}
    ~MouseBinding() override { delete commandName_; }

    MouseButton GetMouseButton() const noexcept { return button_; }
    void SetMouseButton(MouseButton value) noexcept { button_ = value; }
    PointerAction GetAction() const noexcept { return action_; }
    void SetAction(PointerAction value) noexcept { action_ = value; }
    StringView GetCommandName() const noexcept {
        return commandName_ != nullptr ? commandName_->View() : StringView{};
    }
    void SetCommandName(StringView value) noexcept;

    bool Matches(const PointerInput& input) const noexcept;
    bool Finalize() noexcept override;
    void ReleaseAuthored() noexcept {
        delete commandName_;
        commandName_ = nullptr;
    }

private:
    String* commandName_ = nullptr;
    MouseButton button_ = MouseButton::Left;
    PointerAction action_ = PointerAction::Down;
    bool finalized_ = false;
};

} // namespace Aero::Input

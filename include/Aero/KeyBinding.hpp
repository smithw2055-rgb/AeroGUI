#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

class AERO_GUI_API KeyBinding : public Base::Object {
    AERO_DECLARE_TYPE(KeyBinding, Base::Object)
public:
    KeyBinding() noexcept = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    StringView GetKeyName() const noexcept { return keyName_.View(); }
    StringView GetModifiersName() const noexcept { return modifiersName_.View(); }
    Ref<RoutedCommand> GetCommand() const noexcept { return command_; }
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    Result<void> Finalize() noexcept;

private:
    String commandName_;
    String keyName_;
    String modifiersName_;
    Ref<RoutedCommand> command_;
};
} // namespace Aero::Input

#pragma once

#include <Aero/InputBinding.hpp>

namespace Aero::Input {

class AERO_GUI_API KeyBinding : public InputBinding {
    AERO_DECLARE_TYPE(KeyBinding, InputBinding)
public:
    KeyBinding() noexcept : InputBinding(StaticTypeId()) {}
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    StringView GetKeyName() const noexcept { return keyName_.View(); }
    StringView GetModifiersName() const noexcept { return modifiersName_.View(); }
    void SetCommandName(StringView value) noexcept;
    void SetKeyName(StringView value) noexcept;
    void SetModifiersName(StringView value) noexcept;
    Result<void> Finalize() noexcept override;

private:
    String commandName_;
    String keyName_;
    String modifiersName_;
};
} // namespace Aero::Input

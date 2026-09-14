#pragma once

#include <Aero/RoutedCommand.hpp>
#include <Aero/Base/String.hpp>

namespace Aero::Input {

class AERO_GUI_API RoutedUICommand : public RoutedCommand {
    AERO_DECLARE_TYPE(RoutedUICommand, RoutedCommand)
public:
    RoutedUICommand() noexcept = default;
    explicit RoutedUICommand(StringView name) noexcept
        : RoutedCommand(name) {}

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetText() const noexcept { return text_.View(); }
    void SetText(StringView value) noexcept;

private:
    String text_;
};

} // namespace Aero::Input

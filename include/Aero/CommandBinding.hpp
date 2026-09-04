#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/Events/CommandEventArgs.hpp>

namespace Aero::Input {

class AERO_GUI_API CommandBinding : public Base::Object {
    AERO_DECLARE_TYPE(CommandBinding, Base::Object)
public:
    CommandBinding() noexcept = default;
    CommandBinding(
        Ref<RoutedCommand> command,
        const ExecutedRoutedEventHandler& executed,
        const CanExecuteRoutedEventHandler& canExecute = {}) noexcept
        : command_(std::move(command)),
          canExecute_(canExecute),
          executed_(executed) {}

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    RoutedCommand* GetCommand() const noexcept { return command_.Get(); }
    void SetCommand(Ref<RoutedCommand> value) noexcept {
        command_ = std::move(value);
    }
    const CanExecuteRoutedEventHandler& GetCanExecute() const noexcept {
        return canExecute_;
    }
    const ExecutedRoutedEventHandler& GetExecuted() const noexcept {
        return executed_;
    }
    StringView GetCommandName() const noexcept { return commandName_.View(); }
    StringView GetExecutedName() const noexcept { return executedName_.View(); }
    StringView GetCanExecuteName() const noexcept {
        return canExecuteName_.View();
    }
    void SetCommandName(StringView value) noexcept;
    void SetExecutedName(StringView value) noexcept;
    void SetCanExecuteName(StringView value) noexcept;
    Result<void> Finalize() noexcept;
    bool IsValid() const noexcept {
        return command_.Get() != nullptr;
    }

private:
    Ref<RoutedCommand> command_;
    CanExecuteRoutedEventHandler canExecute_;
    ExecutedRoutedEventHandler executed_;
    String commandName_;
    String executedName_;
    String canExecuteName_;
};
} // namespace Aero::Input

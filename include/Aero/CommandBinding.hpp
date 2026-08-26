#pragma once

#include <Aero/RoutedCommand.hpp>
#include <Aero/Events/CommandEventArgs.hpp>

namespace Aero::Input {

class AERO_GUI_API CommandBinding {
public:
    CommandBinding() noexcept = default;
    CommandBinding(
        Ref<RoutedCommand> command,
        const ExecutedRoutedEventHandler& executed,
        const CanExecuteRoutedEventHandler& canExecute = {}) noexcept
        : command_(std::move(command)),
          canExecute_(canExecute),
          executed_(executed) {}

    RoutedCommand* GetCommand() const noexcept { return command_.Get(); }
    const CanExecuteRoutedEventHandler& GetCanExecute() const noexcept {
        return canExecute_;
    }
    const ExecutedRoutedEventHandler& GetExecuted() const noexcept {
        return executed_;
    }
    bool IsValid() const noexcept {
        return command_ && (!canExecute_.Empty() || !executed_.Empty());
    }

private:
    Ref<RoutedCommand> command_;
    CanExecuteRoutedEventHandler canExecute_;
    ExecutedRoutedEventHandler executed_;
};
} // namespace Aero::Input

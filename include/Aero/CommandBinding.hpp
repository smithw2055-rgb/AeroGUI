#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/Events.hpp>
#include <cstdint>

namespace Aero::Input {

struct CommandBindingHandle {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

class AERO_GUI_API CommandBinding : public Base::Object {
    AERO_DECLARE_TYPE(CommandBinding, Base::Object)

public:
    CommandBinding() noexcept = default;
    CommandBinding(Ref<RoutedCommand> command,
        const ExecutedRoutedEventHandler& executed, const CanExecuteRoutedEventHandler& canExecute = {}) noexcept
        : command_(std::move(command)),
          canExecute_(canExecute),
          executed_(executed) {}
    ~CommandBinding() override;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    RoutedCommand* GetCommand() const noexcept { return command_.Get(); }
    void SetCommand(Ref<RoutedCommand> value) noexcept { command_ = std::move(value); }
    const CanExecuteRoutedEventHandler& GetCanExecute() const noexcept { return canExecute_; }
    const ExecutedRoutedEventHandler& GetExecuted() const noexcept { return executed_; }
    StringView GetCommandName() const noexcept;
    StringView GetExecutedName() const noexcept;
    StringView GetCanExecuteName() const noexcept;
    void SetCommandName(StringView value) noexcept;
    void SetExecutedName(StringView value) noexcept;
    void SetCanExecuteName(StringView value) noexcept;
    Result<void> Finalize() noexcept;
    void ReleaseAuthored() noexcept;
    bool IsValid() const noexcept { return command_.Get() != nullptr; }

private:
    struct AuthoredData {
        AuthoredData() noexcept {}
        String commandName;
        String executedName;
        String canExecuteName;
    };
    AuthoredData* EnsureAuthored() noexcept;

    Ref<RoutedCommand> command_;
    CanExecuteRoutedEventHandler canExecute_;
    ExecutedRoutedEventHandler executed_;
    AuthoredData* authored_ = nullptr;
};
} // namespace Aero::Input

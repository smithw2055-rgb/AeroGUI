#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/Events.hpp>
#include <cstdint>
#include <new>

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
        const ExecutedRoutedEventHandler& executed,
        const CanExecuteRoutedEventHandler& canExecute = {}) noexcept
        : command_(std::move(command)),
          executed_(executed) {
        if (!canExecute.Empty()) {
            if (RareData* r = EnsureRare()) {
                r->canExecute = canExecute;
            }
        }
    }
    ~CommandBinding() override { delete rare_; }

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    RoutedCommand* GetCommand() const noexcept { return command_.Get(); }
    void SetCommand(Ref<RoutedCommand> value) noexcept { command_ = std::move(value); }
    const CanExecuteRoutedEventHandler& GetCanExecute() const noexcept {
        return rare_ != nullptr ? rare_->canExecute : EmptyCanExecute();
    }
    const ExecutedRoutedEventHandler& GetExecuted() const noexcept { return executed_; }
    StringView GetCommandName() const noexcept {
        return rare_ != nullptr ? rare_->commandName.View() : StringView{};
    }
    StringView GetExecutedName() const noexcept {
        return rare_ != nullptr ? rare_->executedName.View() : StringView{};
    }
    StringView GetCanExecuteName() const noexcept {
        return rare_ != nullptr ? rare_->canExecuteName.View() : StringView{};
    }
    void SetCommandName(StringView value) noexcept;
    void SetExecutedName(StringView value) noexcept;
    void SetCanExecuteName(StringView value) noexcept;
    bool Finalize() noexcept;
    void ReleaseAuthored() noexcept {
        if (rare_ == nullptr) return;
        if (rare_->canExecute.Empty()) {
            delete rare_;
            rare_ = nullptr;
        } else {
            rare_->commandName.Clear();
            rare_->executedName.Clear();
            rare_->canExecuteName.Clear();
        }
    }
    bool IsValid() const noexcept { return command_.Get() != nullptr; }

    struct ClassBindingEntry {
        Meta::TypeId ownerType = Meta::InvalidTypeId;
        Ref<RoutedCommand> command;
        CanExecuteRoutedEventHandler canExecute;
        ExecutedRoutedEventHandler executed;
    };

    static void RegisterClassBinding(
        Meta::TypeId ownerType,
        const CommandBinding& binding) noexcept;
    static Base::Span<const ClassBindingEntry> ClassBindings() noexcept;

private:
    struct RareData {
        CanExecuteRoutedEventHandler canExecute;
        String commandName{&Base::GetDefaultAllocator()};
        String executedName{&Base::GetDefaultAllocator()};
        String canExecuteName{&Base::GetDefaultAllocator()};
    };

    RareData* EnsureRare() noexcept {
        if (rare_ == nullptr) {
            rare_ = new (std::nothrow) RareData();
        }
        return rare_;
    }

    static const CanExecuteRoutedEventHandler& EmptyCanExecute() noexcept {
        static const CanExecuteRoutedEventHandler kEmpty{};
        return kEmpty;
    }

    Ref<RoutedCommand> command_;
    ExecutedRoutedEventHandler executed_;
    RareData* rare_ = nullptr;
};
} // namespace Aero::Input

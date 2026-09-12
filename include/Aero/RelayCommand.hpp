#pragma once

#include <Aero/ICommand.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Value.hpp>

#include <type_traits>
#include <utility>

namespace Aero::Input {

class AERO_GUI_API RelayCommand : public ICommand {
    AERO_DECLARE_TYPE(RelayCommand, ICommand)
public:
    using ExecuteCallback = Base::Delegate<void(const Value&)>;
    using CanExecuteCallback = Base::Delegate<bool(const Value&)>;
    using SimpleExecuteCallback = Base::Delegate<void()>;
    using SimpleCanExecuteCallback = Base::Delegate<bool()>;

    RelayCommand() noexcept = default;
    ~RelayCommand() override = default;

    explicit RelayCommand(
        SimpleExecuteCallback execute,
        SimpleCanExecuteCallback canExecute = nullptr) noexcept
        : executeSimple_(std::move(execute)),
          canExecuteSimple_(std::move(canExecute)) {}

    explicit RelayCommand(
        ExecuteCallback execute,
        CanExecuteCallback canExecute = nullptr) noexcept
        : execute_(std::move(execute)),
          canExecute_(std::move(canExecute)) {}

    template<class FExec,
        class = std::enable_if_t<
            !std::is_same_v<std::decay_t<FExec>, RelayCommand> &&
            !std::is_base_of_v<ICommand, std::decay_t<FExec>>>>
    explicit RelayCommand(FExec&& execute) noexcept {
        if constexpr (std::is_invocable_v<FExec, const Value&>) {
            execute_ = ExecuteCallback(std::forward<FExec>(execute));
        } else if constexpr (std::is_invocable_v<FExec>) {
            executeSimple_ = SimpleExecuteCallback(std::forward<FExec>(execute));
        }
    }

    template<class FExec, class FCanExec,
        class = std::enable_if_t<!std::is_same_v<std::decay_t<FExec>, RelayCommand>>>
    RelayCommand(FExec&& execute, FCanExec&& canExecute) noexcept {
        if constexpr (std::is_invocable_v<FExec, const Value&>) {
            execute_ = ExecuteCallback(std::forward<FExec>(execute));
        } else if constexpr (std::is_invocable_v<FExec>) {
            executeSimple_ = SimpleExecuteCallback(std::forward<FExec>(execute));
        }
        if constexpr (std::is_invocable_v<FCanExec, const Value&>) {
            canExecute_ = CanExecuteCallback(std::forward<FCanExec>(canExecute));
        } else if constexpr (std::is_invocable_v<FCanExec>) {
            canExecuteSimple_ = SimpleCanExecuteCallback(std::forward<FCanExec>(canExecute));
        }
    }

    Result<bool> CanExecute(
        const Value& parameter,
        UIElement* = nullptr) noexcept override {
        if (canExecute_) {
            return canExecute_(parameter);
        }
        if (canExecuteSimple_) {
            return canExecuteSimple_();
        }
        return true;
    }

    void Execute(
        const Value& parameter,
        UIElement* = nullptr) noexcept override {
        if (execute_) {
            execute_(parameter);
        } else if (executeSimple_) {
            executeSimple_();
        }
    }

    void NotifyCanExecuteChanged() noexcept {
        RaiseCanExecuteChanged();
    }

private:
    ExecuteCallback execute_;
    CanExecuteCallback canExecute_;
    SimpleExecuteCallback executeSimple_;
    SimpleCanExecuteCallback canExecuteSimple_;
};

template<class T>
class RelayCommandT final : public ICommand {
public:
    using Self = RelayCommandT<T>;
    using BaseType = ICommand;
    using ExecuteCallback = Base::Delegate<void(T)>;
    using CanExecuteCallback = Base::Delegate<bool(T)>;

    static constexpr Aero::Base::TypeId StaticTypeId() noexcept {
        return ICommand::StaticTypeId();
    }
    Aero::Base::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    RelayCommandT() noexcept = default;
    ~RelayCommandT() override = default;

    explicit RelayCommandT(
        ExecuteCallback execute,
        CanExecuteCallback canExecute = nullptr) noexcept
        : execute_(std::move(execute)),
          canExecute_(std::move(canExecute)) {}

    template<class FExec,
        class = std::enable_if_t<!std::is_same_v<std::decay_t<FExec>, RelayCommandT>>>
    explicit RelayCommandT(FExec&& execute) noexcept
        : execute_(ExecuteCallback(std::forward<FExec>(execute))) {}

    template<class FExec, class FCanExec>
    RelayCommandT(FExec&& execute, FCanExec&& canExecute) noexcept
        : execute_(ExecuteCallback(std::forward<FExec>(execute))),
          canExecute_(CanExecuteCallback(std::forward<FCanExec>(canExecute))) {}

    Result<bool> CanExecute(
        const Value& parameter,
        UIElement* = nullptr) noexcept override {
        if (!canExecute_) {
            return true;
        }
        Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
        if (!decoded) {
            return false;
        }
        return canExecute_(decoded.Value());
    }

    void Execute(
        const Value& parameter,
        UIElement* = nullptr) noexcept override {
        if (!execute_) {
            return;
        }
        Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
        if (decoded) {
            execute_(decoded.Value());
        }
    }

    void NotifyCanExecuteChanged() noexcept {
        RaiseCanExecuteChanged();
    }

private:
    ExecuteCallback execute_;
    CanExecuteCallback canExecute_;
};

} // namespace Aero::Input

namespace Aero {
using Input::RelayCommand;
using Input::RelayCommandT;
}

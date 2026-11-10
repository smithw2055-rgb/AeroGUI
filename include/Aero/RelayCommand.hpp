#pragma once

#include <Aero/ICommand.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Value.hpp>

#include <type_traits>
#include <utility>

namespace Aero::Input {

class RelayCommand;
template<class T> class RelayCommandT;

template<class FExec>
struct IsCommandExecuteCallable : std::bool_constant<
    !std::is_same_v<std::decay_t<FExec>, RelayCommand> &&
    !std::is_base_of_v<ICommand, std::decay_t<FExec>> &&
    !std::is_member_pointer_v<std::decay_t<FExec>> &&
    (std::is_invocable_v<FExec, const Value&> || std::is_invocable_v<FExec>)> {};

template<class FCanExec>
struct IsCommandCanExecuteCallable : std::bool_constant<
    std::is_same_v<std::decay_t<FCanExec>, std::nullptr_t> ||
    (!std::is_member_pointer_v<std::decay_t<FCanExec>> &&
     (std::is_invocable_r_v<bool, FCanExec, const Value&> || std::is_invocable_r_v<bool, FCanExec>))> {};

class AERO_GUI_API RelayCommand : public ICommand {
    AERO_DECLARE_TYPE(RelayCommand, ICommand)

public:
    using ExecuteCallback = Base::Delegate<void(const Value&)>;
    using CanExecuteCallback = Base::Delegate<bool(const Value&)>;
    using SimpleExecuteCallback = Base::Delegate<void()>;
    using SimpleCanExecuteCallback = Base::Delegate<bool()>;

    RelayCommand() noexcept = default;
    ~RelayCommand() override = default;

    explicit RelayCommand(SimpleExecuteCallback execute, SimpleCanExecuteCallback canExecute = nullptr) noexcept
        : executeSimple_(std::move(execute)),
          canExecuteSimple_(std::move(canExecute)) {}

    explicit RelayCommand(ExecuteCallback execute, CanExecuteCallback canExecute = nullptr) noexcept
        : execute_(std::move(execute)),
          canExecute_(std::move(canExecute)) {}

    template<class FExec, class = std::enable_if_t<IsCommandExecuteCallable<FExec>::value>>
    explicit RelayCommand(FExec&& execute) noexcept {
        if constexpr (std::is_invocable_v<FExec, const Value&>) {
            execute_ = ExecuteCallback(std::forward<FExec>(execute));
        } else {
            static_assert(std::is_invocable_v<FExec>,
                "RelayCommand execute callback must be invocable with (const Value&) or ()");
            executeSimple_ = SimpleExecuteCallback(std::forward<FExec>(execute));
        }
    }

    template<class FExec, class FCanExec, class = std::enable_if_t<
            IsCommandExecuteCallable<FExec>::value &&
            IsCommandCanExecuteCallable<FCanExec>::value>>
    RelayCommand(FExec&& execute, FCanExec&& canExecute) noexcept {
        if constexpr (std::is_invocable_v<FExec, const Value&>) {
            execute_ = ExecuteCallback(std::forward<FExec>(execute));
        } else {
            static_assert(std::is_invocable_v<FExec>,
                "RelayCommand execute callback must be invocable with (const Value&) or ()");
            executeSimple_ = SimpleExecuteCallback(std::forward<FExec>(execute));
        }
        if constexpr (std::is_same_v<std::decay_t<FCanExec>, std::nullptr_t>) {
            (void)canExecute;
        } else if constexpr (std::is_invocable_r_v<bool, FCanExec, const Value&>) {
            canExecute_ = CanExecuteCallback(std::forward<FCanExec>(canExecute));
        } else {
            static_assert(std::is_invocable_r_v<bool, FCanExec>,
                "RelayCommand canExecute callback must return bool and be invocable with (const Value&) or ()");
            canExecuteSimple_ = SimpleCanExecuteCallback(std::forward<FCanExec>(canExecute));
        }
    }

    bool CanExecute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (canExecute_) { return canExecute_(parameter); }
        if (canExecuteSimple_) { return canExecuteSimple_(); }
        return true;
    }

    void Execute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (execute_) {
            execute_(parameter);
        } else if (executeSimple_) { executeSimple_(); }
    }

    void NotifyCanExecuteChanged() noexcept { RaiseCanExecuteChanged(); }

private:
    ExecuteCallback execute_;
    CanExecuteCallback canExecute_;
    SimpleExecuteCallback executeSimple_;
    SimpleCanExecuteCallback canExecuteSimple_;
};

template<class T> class RelayCommandT : public ICommand {
public:
    using Self = RelayCommandT<T>;
    using BaseType = ICommand;
    using ExecuteCallback = Base::Delegate<void(T)>;
    using CanExecuteCallback = Base::Delegate<bool(T)>;

    static constexpr Aero::Base::TypeId StaticTypeId() noexcept { return ICommand::StaticTypeId(); }
    static const ::Aero::Base::TypeClass* StaticClassType() noexcept { return ICommand::StaticClassType(); }
    const ::Aero::Base::TypeClass* GetClassType() const noexcept override { return StaticClassType(); }
    Aero::Base::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    RelayCommandT() noexcept = default;
    ~RelayCommandT() override = default;

    explicit RelayCommandT(ExecuteCallback execute, CanExecuteCallback canExecute = nullptr) noexcept
        : execute_(std::move(execute)),
          canExecute_(std::move(canExecute)) {}

    template<class FExec, class = std::enable_if_t<
            !std::is_same_v<std::decay_t<FExec>, RelayCommandT> &&
            !std::is_member_pointer_v<std::decay_t<FExec>> &&
            std::is_invocable_v<FExec, T>>>
    explicit RelayCommandT(FExec&& execute) noexcept : execute_(ExecuteCallback(std::forward<FExec>(execute))) {}

    template<class FExec, class FCanExec, class = std::enable_if_t<
            !std::is_same_v<std::decay_t<FExec>, RelayCommandT> &&
            !std::is_member_pointer_v<std::decay_t<FExec>> &&
            std::is_invocable_v<FExec, T> &&
            (std::is_same_v<std::decay_t<FCanExec>, std::nullptr_t> ||
             (!std::is_member_pointer_v<std::decay_t<FCanExec>> && std::is_invocable_r_v<bool, FCanExec, T>))>>
    RelayCommandT(FExec&& execute, FCanExec&& canExecute) noexcept
        : execute_(ExecuteCallback(std::forward<FExec>(execute))),
          canExecute_(CanExecuteCallback(std::forward<FCanExec>(canExecute))) {}

    bool CanExecute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (!canExecute_) { return true; }
        Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
        if (!decoded) { return false; }
        return canExecute_(decoded.Value());
    }

    void Execute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (!execute_) { return; }
        Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
        if (decoded) { execute_(decoded.Value()); }
    }

    void NotifyCanExecuteChanged() noexcept { RaiseCanExecuteChanged(); }

private:
    ExecuteCallback execute_;
    CanExecuteCallback canExecute_;
};

struct EmptyCanExecute {};

template<class TOwner, class TExecMethod, class TCanExecMethod = EmptyCanExecute>
class MemberRelayCommand final : public RelayCommand {
public:
    using BareOwner = std::remove_cv_t<TOwner>;
    static constexpr bool kIsObjectOwner = std::is_base_of_v<Base::Object, BareOwner>;
    static constexpr bool kHasCanExecute = !std::is_same_v<TCanExecMethod, EmptyCanExecute>;

    MemberRelayCommand(
        TOwner* owner,
        TExecMethod execMethod,
        TCanExecMethod canExecMethod = {}) noexcept
        : rawOwner_(owner),
          execMethod_(execMethod),
          canExecMethod_(canExecMethod) {
        if constexpr (kIsObjectOwner) {
            if (owner != nullptr) {
                hasWeak_ = weakOwner_.TryAttach(const_cast<BareOwner&>(*owner));
            }
        }
    }

    bool CanExecute(const Value&, UIElement* = nullptr) noexcept override {
        if constexpr (!kHasCanExecute) {
            return true;
        } else {
            if (canExecMethod_ == nullptr) {
                return true;
            }
            if constexpr (kIsObjectOwner) {
                if (hasWeak_) {
                    if (Base::Ref<BareOwner> strong = weakOwner_.Lock()) {
                        return static_cast<bool>((static_cast<TOwner*>(strong.Get())->*canExecMethod_)());
                    }
                    if (BareOwner* raw = weakOwner_.Peek()) {
                        return static_cast<bool>((static_cast<TOwner*>(raw)->*canExecMethod_)());
                    }
                    return false;
                }
            }
            return rawOwner_ != nullptr
                ? static_cast<bool>((rawOwner_->*canExecMethod_)())
                : false;
        }
    }

    void Execute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (execMethod_ == nullptr) {
            return;
        }
        auto invokeOn = [&](TOwner* target) noexcept {
            if constexpr (std::is_invocable_v<TExecMethod, TOwner*, const Value&>) {
                (void)(target->*execMethod_)(parameter);
            } else {
                (void)(target->*execMethod_)();
            }
        };
        if constexpr (kIsObjectOwner) {
            if (hasWeak_) {
                if (Base::Ref<BareOwner> strong = weakOwner_.Lock()) {
                    invokeOn(static_cast<TOwner*>(strong.Get()));
                } else if (BareOwner* raw = weakOwner_.Peek()) {
                    invokeOn(static_cast<TOwner*>(raw));
                }
                return;
            }
        }
        if (rawOwner_ != nullptr) {
            invokeOn(rawOwner_);
        }
    }

private:
    using WeakStorage = std::conditional_t<kIsObjectOwner, Base::WeakRef<BareOwner>, char>;
    WeakStorage weakOwner_{};
    TOwner* rawOwner_ = nullptr;
    TExecMethod execMethod_{};
    TCanExecMethod canExecMethod_{};
    bool hasWeak_ = false;
};

template<class T, class TOwner, class TExecMethod, class TCanExecMethod = EmptyCanExecute>
class MemberRelayCommandT final : public RelayCommandT<T> {
public:
    using BareOwner = std::remove_cv_t<TOwner>;
    static constexpr bool kIsObjectOwner = std::is_base_of_v<Base::Object, BareOwner>;
    static constexpr bool kHasCanExecute = !std::is_same_v<TCanExecMethod, EmptyCanExecute>;

    MemberRelayCommandT(
        TOwner* owner,
        TExecMethod execMethod,
        TCanExecMethod canExecMethod = {}) noexcept
        : rawOwner_(owner),
          execMethod_(execMethod),
          canExecMethod_(canExecMethod) {
        if constexpr (kIsObjectOwner) {
            if (owner != nullptr) {
                hasWeak_ = weakOwner_.TryAttach(const_cast<BareOwner&>(*owner));
            }
        }
    }

    bool CanExecute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if constexpr (!kHasCanExecute) {
            return true;
        } else {
            if (canExecMethod_ == nullptr) {
                return true;
            }
            Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
            if (!decoded) {
                return false;
            }
            if constexpr (kIsObjectOwner) {
                if (hasWeak_) {
                    if (Base::Ref<BareOwner> strong = weakOwner_.Lock()) {
                        return static_cast<bool>((static_cast<TOwner*>(strong.Get())->*canExecMethod_)(decoded.Value()));
                    }
                    if (BareOwner* raw = weakOwner_.Peek()) {
                        return static_cast<bool>((static_cast<TOwner*>(raw)->*canExecMethod_)(decoded.Value()));
                    }
                    return false;
                }
            }
            return rawOwner_ != nullptr
                ? static_cast<bool>((rawOwner_->*canExecMethod_)(decoded.Value()))
                : false;
        }
    }

    void Execute(const Value& parameter, UIElement* = nullptr) noexcept override {
        if (execMethod_ == nullptr) {
            return;
        }
        Result<T> decoded = Meta::ValueCodec<T>::Decode(parameter);
        if (!decoded) {
            return;
        }
        if constexpr (kIsObjectOwner) {
            if (hasWeak_) {
                if (Base::Ref<BareOwner> strong = weakOwner_.Lock()) {
                    (void)(static_cast<TOwner*>(strong.Get())->*execMethod_)(decoded.Value());
                } else if (BareOwner* raw = weakOwner_.Peek()) {
                    (void)(static_cast<TOwner*>(raw)->*execMethod_)(decoded.Value());
                }
                return;
            }
        }
        if (rawOwner_ != nullptr) {
            (void)(rawOwner_->*execMethod_)(decoded.Value());
        }
    }

private:
    using WeakStorage = std::conditional_t<kIsObjectOwner, Base::WeakRef<BareOwner>, char>;
    WeakStorage weakOwner_{};
    TOwner* rawOwner_ = nullptr;
    TExecMethod execMethod_{};
    TCanExecMethod canExecMethod_{};
    bool hasWeak_ = false;
};

template<class FExec, std::enable_if_t<IsCommandExecuteCallable<FExec>::value, int> = 0>
Base::Ref<RelayCommand> MakeCommand(FExec&& execute) noexcept {
    return Base::MakeRef<RelayCommand>(std::forward<FExec>(execute));
}

template<class FExec, class FCanExec, std::enable_if_t<
    IsCommandExecuteCallable<FExec>::value &&
    IsCommandCanExecuteCallable<FCanExec>::value, int> = 0>
Base::Ref<RelayCommand> MakeCommand(FExec&& execute, FCanExec&& canExecute) noexcept {
    return Base::MakeRef<RelayCommand>(
        std::forward<FExec>(execute),
        std::forward<FCanExec>(canExecute));
}

template<class T, class FExec, std::enable_if_t<
    !std::is_member_pointer_v<std::decay_t<FExec>> &&
    std::is_invocable_v<FExec, std::decay_t<T>>, int> = 0>
Base::Ref<RelayCommandT<std::decay_t<T>>> MakeCommand(FExec&& execute) noexcept {
    return Base::MakeRef<RelayCommandT<std::decay_t<T>>>(std::forward<FExec>(execute));
}

template<class T, class FExec, class FCanExec, std::enable_if_t<
    !std::is_member_pointer_v<std::decay_t<FExec>> &&
    std::is_invocable_v<FExec, std::decay_t<T>> &&
    (std::is_same_v<std::decay_t<FCanExec>, std::nullptr_t> ||
     (!std::is_member_pointer_v<std::decay_t<FCanExec>> && std::is_invocable_r_v<bool, FCanExec, std::decay_t<T>>)), int> = 0>
Base::Ref<RelayCommandT<std::decay_t<T>>> MakeCommand(FExec&& execute, FCanExec&& canExecute) noexcept {
    return Base::MakeRef<RelayCommandT<std::decay_t<T>>>(
        std::forward<FExec>(execute),
        std::forward<FCanExec>(canExecute));
}

template<class TOwner, class TMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TMethod> &&
    (std::is_invocable_v<TMethod, TOwner*> || std::is_invocable_v<TMethod, TOwner*, const Value&>), int> = 0>
Base::Ref<RelayCommand> MakeCommand(TOwner* owner, TMethod method) noexcept {
    return Base::Ref<RelayCommand>(
        Base::MakeRef<MemberRelayCommand<TOwner, TMethod>>(owner, method));
}

template<class TOwner, class TExecMethod, class TCanExecMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TExecMethod> &&
    std::is_member_function_pointer_v<TCanExecMethod> &&
    (std::is_invocable_v<TExecMethod, TOwner*> || std::is_invocable_v<TExecMethod, TOwner*, const Value&>) &&
    std::is_invocable_r_v<bool, TCanExecMethod, TOwner*>, int> = 0>
Base::Ref<RelayCommand> MakeCommand(
    TOwner* owner,
    TExecMethod executeMethod,
    TCanExecMethod canExecuteMethod) noexcept {
    return Base::Ref<RelayCommand>(
        Base::MakeRef<MemberRelayCommand<TOwner, TExecMethod, TCanExecMethod>>(
            owner, executeMethod, canExecuteMethod));
}

template<class TOwner, class TMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TMethod> &&
    (std::is_invocable_v<TMethod, TOwner*> || std::is_invocable_v<TMethod, TOwner*, const Value&>), int> = 0>
Base::Ref<RelayCommand> MakeCommand(const Base::Ref<TOwner>& owner, TMethod method) noexcept {
    return MakeCommand(owner.Get(), method);
}

template<class T, class TOwner, class TMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TMethod> &&
    std::is_invocable_v<TMethod, TOwner*, std::decay_t<T>>, int> = 0>
Base::Ref<RelayCommandT<std::decay_t<T>>> MakeCommand(TOwner* owner, TMethod method) noexcept {
    using ValueT = std::decay_t<T>;
    return Base::Ref<RelayCommandT<ValueT>>(
        Base::MakeRef<MemberRelayCommandT<ValueT, TOwner, TMethod>>(owner, method));
}

template<class T, class TOwner, class TExecMethod, class TCanExecMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TExecMethod> &&
    std::is_member_function_pointer_v<TCanExecMethod> &&
    std::is_invocable_v<TExecMethod, TOwner*, std::decay_t<T>> &&
    std::is_invocable_r_v<bool, TCanExecMethod, TOwner*, std::decay_t<T>>, int> = 0>
Base::Ref<RelayCommandT<std::decay_t<T>>> MakeCommand(
    TOwner* owner,
    TExecMethod executeMethod,
    TCanExecMethod canExecuteMethod) noexcept {
    using ValueT = std::decay_t<T>;
    return Base::Ref<RelayCommandT<ValueT>>(
        Base::MakeRef<MemberRelayCommandT<ValueT, TOwner, TExecMethod, TCanExecMethod>>(
            owner, executeMethod, canExecuteMethod));
}

template<class T, class TOwner, class TMethod, std::enable_if_t<
    std::is_member_function_pointer_v<TMethod> &&
    std::is_invocable_v<TMethod, TOwner*, std::decay_t<T>>, int> = 0>
Base::Ref<RelayCommandT<std::decay_t<T>>> MakeCommand(const Base::Ref<TOwner>& owner, TMethod method) noexcept {
    return MakeCommand<std::decay_t<T>>(owner.Get(), method);
}

} // namespace Aero::Input

namespace Aero {
using Input::RelayCommand;
using Input::RelayCommandT;
using Input::MakeCommand;
}

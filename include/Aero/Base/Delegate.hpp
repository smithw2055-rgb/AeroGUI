#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

template<class T>
class Delegate;

namespace Detail {

template<class T>
struct DelegateMulticast;

template<class Ret, class... Args>
struct DelegateOperations final {
    Ret (*invoke)(const void* storage, Args... args);
    void (*copy)(void* destination, const void* source) noexcept;
    void (*destroy)(void* storage) noexcept;
    bool (*equals)(const void* left, const void* right) noexcept;
    bool multicast = false;
};

template<class T>
struct IsDelegate final : std::false_type {};

template<class T>
struct IsDelegate<Delegate<T>> final : std::true_type {};

} // namespace Detail

template<class Ret, class... Args>
class Delegate<Ret(Args...)> final {
public:
    using Signature = Ret(Args...);
    using Operations = Detail::DelegateOperations<Ret, Args...>;
    static constexpr std::size_t StorageSize = 3U * sizeof(void*);

    Delegate() noexcept = default;
    Delegate(std::nullptr_t) noexcept {}

    Delegate(Ret (*function)(Args...)) noexcept {
        if (function != nullptr) Emplace<FreeFunction>(function);
    }

    template<class F,
        class = std::enable_if_t<
            !Detail::IsDelegate<std::decay_t<F>>::value &&
            !std::is_pointer<std::decay_t<F>>::value &&
            std::is_invocable_r<Ret, const F&, Args...>::value>>
    Delegate(const F& function) noexcept {
        static_assert(sizeof(F) <= StorageSize,
            "Delegate functor exceeds the fixed inline storage");
        static_assert(alignof(F) <= alignof(void*),
            "Delegate functor requires unsupported alignment");
        static_assert(std::is_nothrow_copy_constructible<F>::value,
            "Delegate functor must be nothrow copy constructible");
        static_assert(std::is_nothrow_destructible<F>::value,
            "Delegate functor must be nothrow destructible");
        Emplace<OwnedFunctor<F>>(function);
    }

    template<class F,
        class = std::enable_if_t<
            !std::is_function<F>::value &&
            std::is_invocable_r<Ret, const F&, Args...>::value>>
    explicit Delegate(const F* function) noexcept {
        if (function != nullptr) Emplace<FunctorReference<F>>(function);
    }

    template<class C>
    Delegate(C* object, Ret (C::*method)(Args...)) noexcept {
        if (object != nullptr && method != nullptr) {
            Emplace<MemberFunction<C>>(MemberFunction<C>{object, method});
        }
    }

    template<class C>
    Delegate(const C* object, Ret (C::*method)(Args...) const) noexcept {
        if (object != nullptr && method != nullptr) {
            Emplace<ConstMemberFunction<C>>(
                ConstMemberFunction<C>{object, method});
        }
    }

    Delegate(const Delegate& other) noexcept : operations_(other.operations_) {
        if (operations_ != nullptr) operations_->copy(storage_, other.storage_);
    }

    Delegate(Delegate&& other) noexcept : operations_(other.operations_) {
        if (operations_ != nullptr) {
            operations_->copy(storage_, other.storage_);
            other.Reset();
        }
    }

    ~Delegate() noexcept { Reset(); }

    Delegate& operator=(const Delegate& other) noexcept {
        if (this != &other) {
            Reset();
            operations_ = other.operations_;
            if (operations_ != nullptr) {
                operations_->copy(storage_, other.storage_);
            }
        }
        return *this;
    }

    Delegate& operator=(Delegate&& other) noexcept {
        if (this != &other) {
            Reset();
            operations_ = other.operations_;
            if (operations_ != nullptr) {
                operations_->copy(storage_, other.storage_);
                other.Reset();
            }
        }
        return *this;
    }

    void Reset() noexcept {
        if (operations_ != nullptr) operations_->destroy(storage_);
        operations_ = nullptr;
    }

    bool Empty() const noexcept { return operations_ == nullptr; }
    explicit operator bool() const noexcept { return !Empty(); }

    bool operator==(const Delegate& other) const noexcept {
        if (operations_ != other.operations_) return false;
        return operations_ == nullptr || operations_->equals(storage_, other.storage_);
    }
    bool operator!=(const Delegate& other) const noexcept { return !(*this == other); }

    Result<void> TryAdd(
        const Delegate& delegate,
        IAllocator* allocator = nullptr) noexcept;
    void Add(const Delegate& delegate, IAllocator* allocator = nullptr) noexcept;
    void operator+=(const Delegate& delegate) noexcept { Add(delegate); }
    bool Remove(const Delegate& delegate) noexcept;
    void operator-=(const Delegate& delegate) noexcept {
        static_cast<void>(Remove(delegate));
    }

    std::uint32_t Size() const noexcept;

    Ret Invoke(Args... args) const {
        AERO_ASSERT(operations_ != nullptr);
        return operations_->invoke(storage_, std::forward<Args>(args)...);
    }
    Ret operator()(Args... args) const {
        return Invoke(std::forward<Args>(args)...);
    }

private:
    template<class T>
    struct OwnedFunctor final {
        T function;
        std::uintptr_t identity = NextOwnedIdentity();
        Ret operator()(Args... args) const {
            return function(std::forward<Args>(args)...);
        }
        bool operator==(const OwnedFunctor& other) const noexcept {
            return identity == other.identity;
        }
    };

    template<class T>
    struct FunctorReference final {
        const T* function = nullptr;
        Ret operator()(Args... args) const {
            return (*function)(std::forward<Args>(args)...);
        }
        bool operator==(const FunctorReference& other) const noexcept {
            return function == other.function;
        }
    };

    struct FreeFunction final {
        Ret (*function)(Args...) = nullptr;
        Ret operator()(Args... args) const {
            return function(std::forward<Args>(args)...);
        }
        bool operator==(const FreeFunction& other) const noexcept {
            return function == other.function;
        }
    };

    template<class C>
    struct MemberFunction final {
        C* object = nullptr;
        Ret (C::*method)(Args...) = nullptr;
        Ret operator()(Args... args) const {
            return (object->*method)(std::forward<Args>(args)...);
        }
        bool operator==(const MemberFunction& other) const noexcept {
            return object == other.object && method == other.method;
        }
    };

    template<class C>
    struct ConstMemberFunction final {
        const C* object = nullptr;
        Ret (C::*method)(Args...) const = nullptr;
        Ret operator()(Args... args) const {
            return (object->*method)(std::forward<Args>(args)...);
        }
        bool operator==(const ConstMemberFunction& other) const noexcept {
            return object == other.object && method == other.method;
        }
    };

    template<class T>
    static Ret InvokeValue(const void* storage, Args... args) {
        return (*static_cast<const T*>(storage))(
            std::forward<Args>(args)...);
    }

    template<class T>
    static void CopyValue(void* destination, const void* source) noexcept {
        new (destination) T(*static_cast<const T*>(source));
    }

    template<class T>
    static void DestroyValue(void* storage) noexcept {
        static_cast<T*>(storage)->~T();
    }

    template<class T>
    static bool EqualValue(const void* left, const void* right) noexcept {
        return *static_cast<const T*>(left) == *static_cast<const T*>(right);
    }

    template<class T>
    static const Operations& ValueOperations() noexcept {
        static const Operations operations{
            &InvokeValue<T>, &CopyValue<T>, &DestroyValue<T>, &EqualValue<T>, false};
        return operations;
    }

    static std::uintptr_t NextOwnedIdentity() noexcept {
        static std::atomic<std::uintptr_t> next{1U};
        std::uintptr_t identity = next.fetch_add(1U, std::memory_order_relaxed);
        if (identity == 0U) {
            identity = next.fetch_add(1U, std::memory_order_relaxed);
        }
        return identity;
    }

    template<class Wrapper, class Value>
    void Emplace(Value&& value) noexcept {
        static_assert(sizeof(Wrapper) <= StorageSize,
            "Delegate target exceeds fixed inline storage");
        static_assert(alignof(Wrapper) <= alignof(void*),
            "Delegate target requires unsupported alignment");
        new (storage_) Wrapper{std::forward<Value>(value)};
        operations_ = &ValueOperations<Wrapper>();
    }

    static const Operations& MulticastOperations() noexcept;
    Detail::DelegateMulticast<Signature>* Multicast() noexcept;
    const Detail::DelegateMulticast<Signature>* Multicast() const noexcept;
    Result<void> EnsureUniqueMulticast(IAllocator& allocator) noexcept;

    alignas(void*) unsigned char storage_[StorageSize]{};
    const Operations* operations_ = nullptr;

    friend struct Detail::DelegateMulticast<Signature>;
};

namespace Detail {

template<class Ret, class... Args>
struct DelegateMulticast<Ret(Args...)> final {
    std::atomic<std::uint32_t> references{1U};
    IAllocator* allocator = nullptr;
    Vector<Delegate<Ret(Args...)>> delegates;

    explicit DelegateMulticast(IAllocator& source) noexcept
        : allocator(&source), delegates(&source) {}
};

} // namespace Detail

template<class Ret, class... Args>
const typename Delegate<Ret(Args...)>::Operations&
Delegate<Ret(Args...)>::MulticastOperations() noexcept {
    using State = Detail::DelegateMulticast<Signature>;
    static const Operations operations{
        [](const void* storage, Args... args) -> Ret {
            State* state = *static_cast<State* const*>(storage);
            state->references.fetch_add(1U, std::memory_order_relaxed);
            const auto release = [state]() noexcept {
                if (state->references.fetch_sub(
                        1U, std::memory_order_acq_rel) == 1U) {
                    IAllocator* allocator = state->allocator;
                    state->~State();
                    allocator->Deallocate(state, sizeof(State), alignof(State),
                        MemoryTag::General);
                }
            };
            if constexpr (std::is_void<Ret>::value) {
                const std::uint32_t count = state->delegates.Size();
                for (std::uint32_t index = 0U; index < count; ++index) {
                    Delegate current = state->delegates[index];
                    current.Invoke(std::forward<Args>(args)...);
                }
                release();
            } else {
                AERO_ASSERT(!state->delegates.Empty());
                Ret result = state->delegates[0U].Invoke(
                    std::forward<Args>(args)...);
                for (std::uint32_t index = 1U;
                     index < state->delegates.Size(); ++index) {
                    Delegate current = state->delegates[index];
                    result = current.Invoke(std::forward<Args>(args)...);
                }
                release();
                return result;
            }
        },
        [](void* destination, const void* source) noexcept {
            State* state = *static_cast<State* const*>(source);
            state->references.fetch_add(1U, std::memory_order_relaxed);
            *static_cast<State**>(destination) = state;
        },
        [](void* storage) noexcept {
            State* state = *static_cast<State**>(storage);
            if (state->references.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
                IAllocator* allocator = state->allocator;
                state->~State();
                allocator->Deallocate(state, sizeof(State), alignof(State),
                    MemoryTag::General);
            }
        },
        [](const void* left, const void* right) noexcept {
            return *static_cast<State* const*>(left) ==
                *static_cast<State* const*>(right);
        },
        true};
    return operations;
}

template<class Ret, class... Args>
Detail::DelegateMulticast<Ret(Args...)>*
Delegate<Ret(Args...)>::Multicast() noexcept {
    return operations_ != nullptr && operations_->multicast
        ? *reinterpret_cast<Detail::DelegateMulticast<Signature>**>(storage_)
        : nullptr;
}

template<class Ret, class... Args>
const Detail::DelegateMulticast<Ret(Args...)>*
Delegate<Ret(Args...)>::Multicast() const noexcept {
    return operations_ != nullptr && operations_->multicast
        ? *reinterpret_cast<Detail::DelegateMulticast<Signature>* const*>(storage_)
        : nullptr;
}

template<class Ret, class... Args>
Result<void> Delegate<Ret(Args...)>::EnsureUniqueMulticast(
    IAllocator& allocator) noexcept {
    using State = Detail::DelegateMulticast<Signature>;
    State* current = Multicast();
    if (current == nullptr ||
        current->references.load(std::memory_order_acquire) == 1U) {
        return {};
    }
    void* memory = allocator.Allocate(
        {sizeof(State), alignof(State), MemoryTag::General});
    if (memory == nullptr) {
        return Status::Failure(ErrorCode::OutOfMemory,
            "Delegate multicast copy allocation failed");
    }
    State* replacement = new (memory) State(allocator);
    for (const Delegate& item : current->delegates) {
        Result<void> appended = replacement->delegates.TryPushBack(item);
        if (!appended) {
            replacement->~State();
            allocator.Deallocate(memory, sizeof(State), alignof(State),
                MemoryTag::General);
            return appended.GetStatus();
        }
    }
    operations_->destroy(storage_);
    *reinterpret_cast<State**>(storage_) = replacement;
    operations_ = &MulticastOperations();
    return {};
}

template<class Ret, class... Args>
Result<void> Delegate<Ret(Args...)>::TryAdd(
    const Delegate& delegate,
    IAllocator* allocator) noexcept {
    if (delegate.Empty()) return {};
    if (Empty()) {
        *this = delegate;
        return {};
    }
    IAllocator& selected = allocator != nullptr ? *allocator : GetDefaultAllocator();
    using State = Detail::DelegateMulticast<Signature>;
    if (Multicast() == nullptr) {
        void* memory = selected.Allocate(
            {sizeof(State), alignof(State), MemoryTag::General});
        if (memory == nullptr) {
            return Status::Failure(ErrorCode::OutOfMemory,
                "Delegate multicast allocation failed");
        }
        State* state = new (memory) State(selected);
        Result<void> first = state->delegates.TryPushBack(*this);
        Result<void> second = first
            ? state->delegates.TryPushBack(delegate)
            : Result<void>(first.GetStatus());
        if (!second) {
            state->~State();
            selected.Deallocate(memory, sizeof(State), alignof(State),
                MemoryTag::General);
            return second.GetStatus();
        }
        Reset();
        *reinterpret_cast<State**>(storage_) = state;
        operations_ = &MulticastOperations();
        return {};
    }
    Result<void> unique = EnsureUniqueMulticast(selected);
    if (!unique) return unique;
    return Multicast()->delegates.TryPushBack(delegate);
}

template<class Ret, class... Args>
void Delegate<Ret(Args...)>::Add(
    const Delegate& delegate,
    IAllocator* allocator) noexcept {
    Result<void> result = TryAdd(delegate, allocator);
    if (!result) {
        ReportOutOfMemory(sizeof(Detail::DelegateMulticast<Signature>),
            alignof(Detail::DelegateMulticast<Signature>), MemoryTag::General);
    }
}

template<class Ret, class... Args>
bool Delegate<Ret(Args...)>::Remove(const Delegate& delegate) noexcept {
    if (Empty() || delegate.Empty()) return false;
    auto* state = Multicast();
    if (state == nullptr) {
        if (*this != delegate) return false;
        Reset();
        return true;
    }
    Result<void> unique = EnsureUniqueMulticast(*state->allocator);
    if (!unique) {
        ReportOutOfMemory(
            sizeof(Detail::DelegateMulticast<Signature>),
            alignof(Detail::DelegateMulticast<Signature>),
            MemoryTag::General);
    }
    state = Multicast();
    for (std::uint32_t index = state->delegates.Size(); index > 0U; --index) {
        const std::uint32_t current = index - 1U;
        if (state->delegates[current] != delegate) continue;
        for (std::uint32_t next = current + 1U;
             next < state->delegates.Size(); ++next) {
            state->delegates[next - 1U] = std::move(state->delegates[next]);
        }
        state->delegates.PopBack();
        if (state->delegates.Size() == 1U) {
            Delegate survivor = state->delegates[0U];
            Reset();
            *this = std::move(survivor);
        }
        return true;
    }
    return false;
}

template<class Ret, class... Args>
std::uint32_t Delegate<Ret(Args...)>::Size() const noexcept {
    const auto* state = Multicast();
    return state != nullptr ? state->delegates.Size() : (Empty() ? 0U : 1U);
}

template<class C, class Ret, class... Args>
Delegate<Ret(Args...)> MakeDelegate(
    C* object,
    Ret (C::*method)(Args...)) noexcept {
    return Delegate<Ret(Args...)>(object, method);
}

template<class C, class Ret, class... Args>
Delegate<Ret(Args...)> MakeDelegate(
    const C* object,
    Ret (C::*method)(Args...) const) noexcept {
    return Delegate<Ret(Args...)>(object, method);
}

} // namespace Aero::Base

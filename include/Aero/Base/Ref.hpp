#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

template<class T>
class Ref  {

public:
    constexpr Ref() noexcept = default;
    constexpr Ref(std::nullptr_t) noexcept {}

    Ref(const Ref& other) noexcept
        : value_(other.value_) {
        AddReference();
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    Ref(const Ref<U>& other) noexcept
        : value_(other.Get()) {
        AddReference();
    }

    Ref(Ref&& other) noexcept
        : value_(other.value_) {
        other.value_ = nullptr;
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    Ref(Ref<U>&& other) noexcept
        : value_(other.Detach()) {}

    ~Ref() {
        Reset();
    }

    Ref& operator=(const Ref& other) noexcept {
        if (this != &other) {
            Ref temporary(other);
            Swap(temporary);
        }
        return *this;
    }

    Ref& operator=(Ref&& other) noexcept {
        if (this != &other) {
            Reset();
            value_ = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }

    T* Get() const noexcept { return value_; }
    T& operator*() const noexcept {
        AERO_ASSERT(value_ != nullptr);
        return *value_;
    }
    T* operator->() const noexcept {
        AERO_ASSERT(value_ != nullptr);
        return value_;
    }
    explicit operator bool() const noexcept { return value_ != nullptr; }

    // Acquires a strong reference to an already-owned object. This is useful
    // for transactional bookkeeping that must outlive a caller's temporary
    // ownership until the transaction is committed or discarded.
    static Ref FromBorrowed(T& value) noexcept {
        reinterpret_cast<Object*>(&value)->AddRef();
        return Ref(&value, AdoptRef);
    }

    // Returns an empty reference for stack/embedded Objects that do not have an
    // intrusive control block. This lets snapshot code strongly retain managed
    // objects while remaining source-compatible with stack-based test hosts.
    static Ref TryFromBorrowed(T& value) noexcept {
        Object* base = reinterpret_cast<Object*>(&value);
        if (!base->TryAddStrongReference()) {
            return {};
        }
        return Ref(&value, AdoptRef);
    }

    void Reset() noexcept {
        T* value = value_;
        value_ = nullptr;
        if (value != nullptr) {
            reinterpret_cast<Object*>(value)->Release();
        }
    }

    void Swap(Ref& other) noexcept {
        T* temporary = value_;
        value_ = other.value_;
        other.value_ = temporary;
    }

    T* Detach() noexcept {
        T* value = value_;
        value_ = nullptr;
        return value;
    }

private:
    explicit Ref(T* value, AdoptRefTag) noexcept
        : value_(value) {}

    void AddReference() noexcept {
        if (value_ != nullptr) {
            reinterpret_cast<Object*>(value_)->AddRef();
        }
    }

    T* value_ = nullptr;

    template<class U>
    friend class Ref;

    template<class U>
    friend class WeakRef;

    template<class U, class... Args>
    friend Result<Ref<U>> MakeRefWithAllocator(IAllocator&, Args&&...) noexcept;
};

template<class T>
class WeakRef : private WeakRefBase {
    static_assert(std::is_base_of<Object, T>::value,
        "WeakRef<T> requires T to derive from Aero::Base::Object");

public:
    constexpr WeakRef() noexcept = default;

    WeakRef(const Ref<T>& strong) noexcept {
        AttachStrong(strong.Get());
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    WeakRef(const Ref<U>& strong) noexcept {
        AttachStrong(strong.Get());
    }

    WeakRef(const WeakRef& other) noexcept
        : WeakRefBase(static_cast<const WeakRefBase&>(other)) {}

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    WeakRef(const WeakRef<U>& other) noexcept
        : WeakRefBase(static_cast<const WeakRefBase&>(other)) {}

    WeakRef(WeakRef&& other) noexcept
        : WeakRefBase(static_cast<WeakRefBase&&>(other)) {}

    ~WeakRef() noexcept = default;

    WeakRef& operator=(const WeakRef& other) noexcept {
        if (this != &other) {
            WeakRef temporary(other);
            Swap(temporary);
        }
        return *this;
    }

    WeakRef& operator=(WeakRef&& other) noexcept {
        if (this != &other) {
            WeakRef temporary(std::move(other));
            Swap(temporary);
        }
        return *this;
    }

    void Reset() noexcept { WeakRefBase::Reset(); }

    void Swap(WeakRef& other) noexcept { WeakRefBase::Swap(other); }

    bool Expired() const noexcept { return WeakRefBase::Expired(); }

    Ref<T> Lock() const noexcept {
        Object* object = WeakRefBase::LockObject();
        if (object == nullptr) {
            return {};
        }
        return Ref<T>(static_cast<T*>(object), AdoptRef);
    }

    template<class U>
    friend class WeakRef;

private:
    void AttachStrong(T* value) noexcept {
        if (value != nullptr) {
            Attach(*value);
        }
    }
};

template<class T, class... Args>
Result<Ref<T>> MakeRefWithAllocator(
    IAllocator& allocator, Args&&... args) noexcept {
    static_assert(std::is_base_of<Object, T>::value,
        "MakeRef<T> requires T to derive from Aero::Base::Object");
    static_assert(std::is_nothrow_constructible<T, Args...>::value,
        "Aero runtime objects must be nothrow constructible");
    static_assert(std::is_destructible<T>::value,
        "Aero runtime objects must have an accessible destructor");

    constexpr std::size_t objectSize = sizeof(T);
    constexpr std::size_t objectAlignment = alignof(T);

    void* memory = allocator.Allocate(
        {objectSize, objectAlignment, MemoryTag::Object});
    if (memory == nullptr) {
        return Status::Failure(ErrorCode::OutOfMemory,
            "Unable to allocate Aero object");
    }

    T* object = new (memory) T(std::forward<Args>(args)...);

    auto destroy = [](Object* base,
                      IAllocator* objectAllocator,
                      std::size_t size,
                      std::size_t alignment) noexcept {
        T* typed = static_cast<T*>(base);
        typed->~T();
        objectAllocator->Deallocate(
            typed, size, alignment, MemoryTag::Object);
    };

    if (!object->AttachManagedLifetime(
            allocator, destroy, objectSize, objectAlignment)) {
        object->~T();
        allocator.Deallocate(
            object, objectSize, objectAlignment, MemoryTag::Object);
        return Status::Failure(ErrorCode::OutOfMemory,
            "Unable to allocate Aero object control block");
    }

    return Ref<T>(object, AdoptRef);
}

template<class T, class... Args>
Result<Ref<T>> MakeRef(Args&&... args) noexcept {
    return MakeRefWithAllocator<T>(
        GetDefaultAllocator(), std::forward<Args>(args)...);
}

} // namespace Aero::Base

namespace Aero {

template<class T>
using Ref = Base::Ref<T>;

using Base::MakeRef;
using Base::MakeRefWithAllocator;

template<class T, class... Args>
inline Ref<T> New(Args&&... args) noexcept {
    auto res = Base::MakeRef<T>(std::forward<Args>(args)...);
    return res ? res.Value() : Ref<T>{};
}

} // namespace Aero
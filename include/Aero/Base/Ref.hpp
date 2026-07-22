#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

namespace Detail {
struct AdoptRefTag final {};
inline constexpr AdoptRefTag AdoptRef{};
} // namespace Detail

template<class T>
class Ref final {
    static_assert(std::is_base_of<Object, T>::value,
        "Ref<T> requires T to derive from Aero::Base::Object");

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

    AERO_NODISCARD T* Get() const noexcept { return value_; }
    AERO_NODISCARD T& operator*() const noexcept {
        AERO_ASSERT(value_ != nullptr);
        return *value_;
    }
    AERO_NODISCARD T* operator->() const noexcept {
        AERO_ASSERT(value_ != nullptr);
        return value_;
    }
    AERO_NODISCARD explicit operator bool() const noexcept { return value_ != nullptr; }

    // Acquires a strong reference to an already-owned object. This is useful
    // for transactional bookkeeping that must outlive a caller's temporary
    // ownership until the transaction is committed or discarded.
    AERO_NODISCARD static Ref FromBorrowed(T& value) noexcept {
        value.AddRef();
        return Ref(&value, Detail::AdoptRef);
    }

    void Reset() noexcept {
        T* value = value_;
        value_ = nullptr;
        if (value != nullptr) {
            value->Release();
        }
    }

    void Swap(Ref& other) noexcept {
        T* temporary = value_;
        value_ = other.value_;
        other.value_ = temporary;
    }

    AERO_NODISCARD T* Detach() noexcept {
        T* value = value_;
        value_ = nullptr;
        return value;
    }

private:
    explicit Ref(T* value, Detail::AdoptRefTag) noexcept
        : value_(value) {}

    void AddReference() noexcept {
        if (value_ != nullptr) {
            value_->AddRef();
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
class WeakRef final {
    static_assert(std::is_base_of<Object, T>::value,
        "WeakRef<T> requires T to derive from Aero::Base::Object");

public:
    constexpr WeakRef() noexcept = default;

    WeakRef(const Ref<T>& strong) noexcept {
        Attach(strong.Get());
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    WeakRef(const Ref<U>& strong) noexcept {
        Attach(strong.Get());
    }

    WeakRef(const WeakRef& other) noexcept
        : control_(other.control_) {
        AddWeakReference();
    }

    template<class U,
        class = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    WeakRef(const WeakRef<U>& other) noexcept
        : control_(other.control_) {
        AddWeakReference();
    }

    WeakRef(WeakRef&& other) noexcept
        : control_(other.control_) {
        other.control_ = nullptr;
    }

    ~WeakRef() {
        Reset();
    }

    WeakRef& operator=(const WeakRef& other) noexcept {
        if (this != &other) {
            WeakRef temporary(other);
            Swap(temporary);
        }
        return *this;
    }

    WeakRef& operator=(WeakRef&& other) noexcept {
        if (this != &other) {
            Reset();
            control_ = other.control_;
            other.control_ = nullptr;
        }
        return *this;
    }

    void Reset() noexcept {
        Detail::ObjectControlBlock* control = control_;
        control_ = nullptr;
        if (control != nullptr) {
            Detail::ReleaseWeak(control);
        }
    }

    void Swap(WeakRef& other) noexcept {
        Detail::ObjectControlBlock* temporary = control_;
        control_ = other.control_;
        other.control_ = temporary;
    }

    AERO_NODISCARD bool Expired() const noexcept {
        return control_ == nullptr || Detail::GetStrongCount(control_) == 0U;
    }

    AERO_NODISCARD Ref<T> Lock() const noexcept {
        if (control_ == nullptr || !Detail::TryAddStrong(control_)) {
            return {};
        }

        Object* object = Detail::GetObject(control_);
        AERO_ASSERT(object != nullptr);
        return Ref<T>(static_cast<T*>(object), Detail::AdoptRef);
    }

private:
    void Attach(Object* object) noexcept {
        if (object != nullptr) {
            control_ = object->ControlBlock();
            AERO_ASSERT(control_ != nullptr);
            Detail::AddWeak(control_);
        }
    }

    void AddWeakReference() noexcept {
        if (control_ != nullptr) {
            Detail::AddWeak(control_);
        }
    }

    Detail::ObjectControlBlock* control_ = nullptr;

    template<class U>
    friend class WeakRef;
};

template<class T, class... Args>
AERO_NODISCARD Result<Ref<T>> MakeRefWithAllocator(
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

    Detail::ObjectControlBlock* control = Detail::CreateObjectControlBlock(
        allocator, object, objectSize, objectAlignment, destroy);
    if (control == nullptr) {
        object->~T();
        allocator.Deallocate(
            object, objectSize, objectAlignment, MemoryTag::Object);
        return Status::Failure(ErrorCode::OutOfMemory,
            "Unable to allocate Aero object control block");
    }

    object->AttachControlBlock(control);
    return Ref<T>(object, Detail::AdoptRef);
}

template<class T, class... Args>
AERO_NODISCARD Result<Ref<T>> MakeRef(Args&&... args) noexcept {
    return MakeRefWithAllocator<T>(
        GetDefaultAllocator(), std::forward<Args>(args)...);
}

} // namespace Aero::Base

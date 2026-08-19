#include <Aero/Base/Object.hpp>
#include <Aero/Base/Assert.hpp>

#include <atomic>
#include <limits>
#include <new>

namespace Aero::Base {
namespace {

struct ObjectControlBlock {
    std::atomic<std::uint32_t> strong{1U};
    std::atomic<std::uint32_t> weak{1U};
    std::atomic<Object*> object{nullptr};
    IAllocator* allocator = nullptr;
    std::size_t objectSize = 0;
    std::size_t objectAlignment = 0;
    DestroyObjectFn destroy = nullptr;
};

void ReleaseWeak(ObjectControlBlock* control) noexcept;

ObjectControlBlock* CreateObjectControlBlock(
    IAllocator& allocator,
    Object* object,
    std::size_t objectSize,
    std::size_t objectAlignment,
    DestroyObjectFn destroy) noexcept {
    AERO_ASSERT(object != nullptr);
    AERO_ASSERT(destroy != nullptr);

    void* memory = allocator.Allocate({
        sizeof(ObjectControlBlock),
        alignof(ObjectControlBlock),
        MemoryTag::WeakControlBlock});
    if (memory == nullptr) {
        return nullptr;
    }

    auto* control = new (memory) ObjectControlBlock();
    control->object.store(object, std::memory_order_release);
    control->allocator = &allocator;
    control->objectSize = objectSize;
    control->objectAlignment = objectAlignment;
    control->destroy = destroy;
    return control;
}

void AddStrong(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    const std::uint32_t previous =
        control->strong.fetch_add(1U, std::memory_order_relaxed);
    AERO_ASSERT(previous > 0U);
    AERO_ASSERT(previous < std::numeric_limits<std::uint32_t>::max());
    (void)previous;
}

bool AcquireStrong(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);

    std::uint32_t count = control->strong.load(std::memory_order_acquire);
    while (count != 0U) {
        AERO_ASSERT(count < std::numeric_limits<std::uint32_t>::max());
        if (control->strong.compare_exchange_weak(
                count,
                count + 1U,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void ReleaseStrong(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    const std::uint32_t previous =
        control->strong.fetch_sub(1U, std::memory_order_acq_rel);
    AERO_ASSERT(previous > 0U);

    if (previous != 1U) {
        return;
    }

    Object* object = control->object.exchange(nullptr, std::memory_order_acq_rel);
    AERO_ASSERT(object != nullptr);
    AERO_ASSERT(control->destroy != nullptr);
    control->destroy(
        object,
        control->allocator,
        control->objectSize,
        control->objectAlignment);

    ReleaseWeak(control);
}

void AddWeak(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    const std::uint32_t previous =
        control->weak.fetch_add(1U, std::memory_order_relaxed);
    AERO_ASSERT(previous > 0U);
    AERO_ASSERT(previous < std::numeric_limits<std::uint32_t>::max());
    (void)previous;
}

void ReleaseWeak(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    const std::uint32_t previous =
        control->weak.fetch_sub(1U, std::memory_order_acq_rel);
    AERO_ASSERT(previous > 0U);

    if (previous != 1U) {
        return;
    }

    IAllocator* allocator = control->allocator;
    control->~ObjectControlBlock();
    allocator->Deallocate(
        control,
        sizeof(ObjectControlBlock),
        alignof(ObjectControlBlock),
        MemoryTag::WeakControlBlock);
}

Object* GetObject(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    return control->object.load(std::memory_order_acquire);
}

std::uint32_t GetStrongCount(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    return control->strong.load(std::memory_order_acquire);
}

} // namespace

void Object::AddRef() noexcept {
    AERO_ASSERT(control_ != nullptr);
    AddStrong(static_cast<ObjectControlBlock*>(control_));
}

void Object::Release() noexcept {
    AERO_ASSERT(control_ != nullptr);
    ReleaseStrong(static_cast<ObjectControlBlock*>(control_));
}

std::uint32_t Object::UseCount() const noexcept {
    AERO_ASSERT(control_ != nullptr);
    return GetStrongCount(static_cast<ObjectControlBlock*>(control_));
}

bool Object::TryAddStrongReference() noexcept {
    if (control_ == nullptr) {
        return false;
    }
    return AcquireStrong(static_cast<ObjectControlBlock*>(control_));
}

bool Object::AttachManagedLifetime(
    IAllocator& allocator,
    DestroyObjectFn destroy,
    std::size_t objectSize,
    std::size_t objectAlignment) noexcept {
    AERO_ASSERT(control_ == nullptr);
    ObjectControlBlock* control = CreateObjectControlBlock(
        allocator, this, objectSize, objectAlignment, destroy);
    if (control == nullptr) {
        return false;
    }
    control_ = control;
    return true;
}

WeakRefBase::WeakRefBase(const WeakRefBase& other) noexcept
    : control_(other.control_) {
    if (control_ != nullptr) {
        AddWeak(static_cast<ObjectControlBlock*>(control_));
    }
}

WeakRefBase::WeakRefBase(WeakRefBase&& other) noexcept
    : control_(other.control_) {
    other.control_ = nullptr;
}

WeakRefBase::~WeakRefBase() noexcept {
    Reset();
}

WeakRefBase& WeakRefBase::operator=(const WeakRefBase& other) noexcept {
    if (this != &other) {
        WeakRefBase temporary(other);
        Swap(temporary);
    }
    return *this;
}

WeakRefBase& WeakRefBase::operator=(WeakRefBase&& other) noexcept {
    if (this != &other) {
        Reset();
        control_ = other.control_;
        other.control_ = nullptr;
    }
    return *this;
}

void WeakRefBase::Reset() noexcept {
    void* control = control_;
    control_ = nullptr;
    if (control != nullptr) {
        ReleaseWeak(static_cast<ObjectControlBlock*>(control));
    }
}

void WeakRefBase::Swap(WeakRefBase& other) noexcept {
    void* temporary = control_;
    control_ = other.control_;
    other.control_ = temporary;
}

bool WeakRefBase::Expired() const noexcept {
    return control_ == nullptr
        || GetStrongCount(static_cast<ObjectControlBlock*>(control_)) == 0U;
}

Object* WeakRefBase::LockObject() const noexcept {
    if (control_ == nullptr) {
        return nullptr;
    }
    ObjectControlBlock* control =
        static_cast<ObjectControlBlock*>(control_);
    if (!AcquireStrong(control)) {
        return nullptr;
    }
    Object* object = GetObject(control);
    AERO_ASSERT(object != nullptr);
    return object;
}

void WeakRefBase::Attach(Object& object) noexcept {
    void* control = object.control_;
    AERO_ASSERT(control != nullptr);
    control_ = control;
    AddWeak(static_cast<ObjectControlBlock*>(control));
}

} // namespace Aero::Base
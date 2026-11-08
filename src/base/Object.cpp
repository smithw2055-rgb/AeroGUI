#include <Aero/Base/Object.hpp>
#include <Aero/Base/Assert.hpp>

#include <atomic>
#include <limits>
#include <new>

namespace Aero::Base {
namespace {

constexpr std::uint32_t ExpiredLockBit = 0x80000000U;

struct ObjectControlBlock {
    std::atomic<std::uint32_t> weak{1U};
    std::atomic<std::uint32_t> lockState{0U};
    std::atomic<Object*> object{nullptr};
    IAllocator* allocator = nullptr;
};

ObjectControlBlock* CreateObjectControlBlock(
    IAllocator& allocator,
    Object* object) noexcept {
    AERO_ASSERT(object != nullptr);

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
    return control;
}

void DestroyObjectControlBlock(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    IAllocator* allocator = control->allocator;
    AERO_ASSERT(allocator != nullptr);
    control->~ObjectControlBlock();
    allocator->Deallocate(
        control,
        sizeof(ObjectControlBlock),
        alignof(ObjectControlBlock),
        MemoryTag::WeakControlBlock);
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

    DestroyObjectControlBlock(control);
}

bool AcquireLockGuard(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    std::uint32_t state = control->lockState.load(std::memory_order_acquire);
    while ((state & ExpiredLockBit) == 0U) {
        AERO_ASSERT((state & ~ExpiredLockBit) < (ExpiredLockBit - 1U));
        if (control->lockState.compare_exchange_weak(
                state,
                state + 1U,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void ReleaseLockGuard(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    const std::uint32_t previous =
        control->lockState.fetch_sub(1U, std::memory_order_release);
    AERO_ASSERT((previous & ~ExpiredLockBit) > 0U);
    (void)previous;
}

void ExpireControlBlock(ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control != nullptr);
    control->object.store(nullptr, std::memory_order_release);
    const std::uint32_t previous =
        control->lockState.fetch_or(ExpiredLockBit, std::memory_order_acq_rel);
    AERO_ASSERT((previous & ExpiredLockBit) == 0U);
    (void)previous;
    while ((control->lockState.load(std::memory_order_acquire) & ~ExpiredLockBit) != 0U) {
    }
}

} // namespace

Object::~Object() {
    ExpireWeakReferences();
}

void Object::ExpireWeakReferences() noexcept {
    void* rawControl = control_.load(std::memory_order_relaxed);
    if (rawControl != nullptr) {
        rawControl = control_.exchange(nullptr, std::memory_order_acq_rel);
        if (rawControl != nullptr) {
            auto* control = static_cast<ObjectControlBlock*>(rawControl);
            ExpireControlBlock(control);
            ReleaseWeak(control);
        }
    }
}

void Object::AddRef() noexcept {
    const std::uint32_t previous =
        strongCount_.fetch_add(1U, std::memory_order_relaxed);
    AERO_ASSERT(previous > 0U);
    AERO_ASSERT(previous < std::numeric_limits<std::uint32_t>::max());
    (void)previous;
}

void Object::Release() noexcept {
    const std::uint32_t previous =
        strongCount_.fetch_sub(1U, std::memory_order_acq_rel);
    AERO_ASSERT(previous > 0U);

    if (previous != 1U) {
        return;
    }

    void* rawControl = control_.exchange(nullptr, std::memory_order_acq_rel);
    auto* control = static_cast<ObjectControlBlock*>(rawControl);
    if (control != nullptr) {
        ExpireControlBlock(control);
        ReleaseWeak(control);
    }

    if ((coreTypeMask_ & kManagedSystemAllocator) != 0U) {
        this->~Object();
        GetSystemAllocator().Deallocate(this, 0U, alignof(Object), MemoryTag::Object);
        return;
    }
    AERO_ASSERT((coreTypeMask_ & kManagedCustomAllocator) != 0U);
    const CustomAllocationHeader* header =
        reinterpret_cast<const CustomAllocationHeader*>(this) - 1;
    DestroyObjectFn destroy = header->destroy;
    IAllocator* allocator = header->allocator;
    AERO_ASSERT(destroy != nullptr);
    destroy(this, allocator);
}

std::uint32_t Object::UseCount() const noexcept {
    return strongCount_.load(std::memory_order_acquire);
}

bool Object::TryAddStrongReference() noexcept {
    std::uint32_t count = strongCount_.load(std::memory_order_acquire);
    while (count != 0U) {
        AERO_ASSERT(count < std::numeric_limits<std::uint32_t>::max());
        if (strongCount_.compare_exchange_weak(
                count,
                count + 1U,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void Object::PromoteConstructingObject() noexcept {
    std::uint32_t expected = 0U;
    (void)strongCount_.compare_exchange_strong(
        expected,
        1U,
        std::memory_order_acq_rel,
        std::memory_order_relaxed);
}

void Object::AttachSystemManagedLifetime() noexcept {
    coreTypeMask_ = static_cast<std::uint16_t>(
        (coreTypeMask_ & kCoreTypeMaskBits) | kManagedSystemAllocator);
    std::uint32_t expected = 0U;
    (void)strongCount_.compare_exchange_strong(
        expected,
        1U,
        std::memory_order_release,
        std::memory_order_relaxed);
}

bool Object::AttachManagedLifetime(
    IAllocator& allocator,
    DestroyObjectFn destroy) noexcept {
    AERO_ASSERT(destroy != nullptr);
    CustomAllocationHeader* header =
        reinterpret_cast<CustomAllocationHeader*>(this) - 1;
    header->allocator = &allocator;
    header->destroy = destroy;
    coreTypeMask_ = static_cast<std::uint16_t>(
        (coreTypeMask_ & kCoreTypeMaskBits) | kManagedCustomAllocator);
    std::uint32_t expected = 0U;
    (void)strongCount_.compare_exchange_strong(
        expected,
        1U,
        std::memory_order_release,
        std::memory_order_relaxed);
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
    if (control_ == nullptr) {
        return true;
    }
    auto* control = static_cast<ObjectControlBlock*>(control_);
    if (!AcquireLockGuard(control)) {
        return true;
    }
    Object* object = control->object.load(std::memory_order_acquire);
    const bool expired = (object == nullptr) ||
        ((object->coreTypeMask_ & Object::kManagedLifetimeMask) != 0U && object->UseCount() == 0U);
    ReleaseLockGuard(control);
    return expired;
}

Object* WeakRefBase::LockObject() const noexcept {
    if (control_ == nullptr) {
        return nullptr;
    }
    auto* control = static_cast<ObjectControlBlock*>(control_);
    if (!AcquireLockGuard(control)) {
        return nullptr;
    }
    Object* object = control->object.load(std::memory_order_acquire);
    const bool acquired =
        (object != nullptr) && object->TryAddStrongReference();
    ReleaseLockGuard(control);
    return acquired ? object : nullptr;
}

Object* WeakRefBase::PeekObject() const noexcept {
    if (control_ == nullptr) {
        return nullptr;
    }
    auto* control = static_cast<ObjectControlBlock*>(control_);
    if (!AcquireLockGuard(control)) {
        return nullptr;
    }
    Object* object = control->object.load(std::memory_order_acquire);
    if (object != nullptr && (object->coreTypeMask_ & Object::kManagedLifetimeMask) != 0U && object->UseCount() == 0U) {
        object = nullptr;
    }
    ReleaseLockGuard(control);
    return object;
}

void WeakRefBase::Attach(Object& object) noexcept {
    AERO_ASSERT(object.UseCount() > 0U);
    if (object.UseCount() == 0U) {
        if (control_ != nullptr) {
            Reset();
        }
        return;
    }
    const bool attached = TryAttach(object);
    AERO_ASSERT(attached);
    (void)attached;
}

bool WeakRefBase::TryAttach(Object& object) noexcept {
    if (control_ != nullptr) {
        Reset();
    }
    const std::uint16_t managedFlags =
        static_cast<std::uint16_t>(object.coreTypeMask_ & Object::kManagedLifetimeMask);
    if (managedFlags != Object::kManagedNone && object.UseCount() == 0U) {
        return false;
    }

    void* existing = object.control_.load(std::memory_order_acquire);
    auto* control = static_cast<ObjectControlBlock*>(existing);
    if (control == nullptr) {
        IAllocator* objectAlloc = nullptr;
        if ((managedFlags & Object::kManagedSystemAllocator) != 0U) {
            objectAlloc = &GetSystemAllocator();
        } else if ((managedFlags & Object::kManagedCustomAllocator) != 0U) {
            const auto* header =
                reinterpret_cast<const Object::CustomAllocationHeader*>(&object) - 1;
            objectAlloc = header->allocator;
        }
        IAllocator& allocator = objectAlloc != nullptr
            ? *objectAlloc
            : GetDefaultAllocator();
        ObjectControlBlock* candidate =
            CreateObjectControlBlock(allocator, &object);
        if (candidate == nullptr) {
            return false;
        }
        if (object.control_.compare_exchange_strong(
                existing,
                candidate,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            control = candidate;
        } else {
            DestroyObjectControlBlock(candidate);
            control = static_cast<ObjectControlBlock*>(existing);
        }
    }

    if (control == nullptr) {
        return false;
    }
    control_ = control;
    AddWeak(control);
    return true;
}

} // namespace Aero::Base
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Assert.hpp>

#include <atomic>
#include <limits>
#include <new>

namespace Aero::Base::Detail {

struct ObjectControlBlock final {
    std::atomic<std::uint32_t> strong{1U};
    std::atomic<std::uint32_t> weak{1U};
    std::atomic<Object*> object{nullptr};
    IAllocator* allocator = nullptr;
    std::size_t objectSize = 0;
    std::size_t objectAlignment = 0;
    DestroyObjectFn destroy = nullptr;
};

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
}

bool TryAddStrong(ObjectControlBlock* control) noexcept {
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

} // namespace Aero::Base::Detail

namespace Aero::Base {

void Object::AddRef() noexcept {
    AERO_ASSERT(control_ != nullptr);
    Detail::AddStrong(control_);
}

bool Object::TryAddRef() noexcept {
    AERO_ASSERT(control_ != nullptr);
    return Detail::TryAddStrong(control_);
}

void Object::Release() noexcept {
    AERO_ASSERT(control_ != nullptr);
    Detail::ObjectControlBlock* control = control_;
    Detail::ReleaseStrong(control);
}

std::uint32_t Object::UseCount() const noexcept {
    AERO_ASSERT(control_ != nullptr);
    return Detail::GetStrongCount(control_);
}

void Object::AttachControlBlock(Detail::ObjectControlBlock* control) noexcept {
    AERO_ASSERT(control_ == nullptr);
    AERO_ASSERT(control != nullptr);
    control_ = control;
}

Detail::ObjectControlBlock* Object::ControlBlock() const noexcept {
    return control_;
}

} // namespace Aero::Base

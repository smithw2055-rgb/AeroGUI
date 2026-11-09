#include <Aero/Base/Object.hpp>
#include <Aero/Base/Assert.hpp>

#include <atomic>
#include <limits>
#include <new>

namespace Aero::Base {
namespace {

constexpr std::uint32_t ExpiredLockBit = 0x80000000U;
constexpr std::uint32_t kMaxTypeClasses = 1024U;
constexpr std::uint32_t kTypeClassHashCapacity = 2048U;
constexpr std::uint32_t kTypeClassHashMask = kTypeClassHashCapacity - 1U;

std::atomic_flag g_typeClassLock = ATOMIC_FLAG_INIT;
const TypeClass* g_typeClassesByIndex[kMaxTypeClasses]{};
MetaTypeId g_denseIdByIndex[kMaxTypeClasses]{};
MetaTypeId g_typeHashKeys[kTypeClassHashCapacity]{};
std::uint16_t g_typeHashIndices[kTypeClassHashCapacity]{};
std::uint16_t g_typeClassCount = 0U;
const TypeClass* g_rootTypeClasses = nullptr;

struct TypeClassSpinGuard {
    TypeClassSpinGuard() noexcept {
        while (g_typeClassLock.test_and_set(std::memory_order_acquire)) {
        }
    }
    ~TypeClassSpinGuard() noexcept {
        g_typeClassLock.clear(std::memory_order_release);
    }
};

constexpr std::uint32_t HashTypeId(MetaTypeId typeId) noexcept {
    std::uint64_t x = typeId;
    x ^= x >> 33U;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33U;
    return static_cast<std::uint32_t>(x) & kTypeClassHashMask;
}

std::uint16_t FindOrCreateDenseIndexLocked(MetaTypeId typeId) noexcept {
    if (typeId == InvalidMetaTypeId) {
        return 0U;
    }
    std::uint32_t slot = HashTypeId(typeId);
    for (std::uint32_t probe = 0U; probe < kTypeClassHashCapacity; ++probe) {
        const MetaTypeId existing = g_typeHashKeys[slot];
        if (existing == typeId) {
            return g_typeHashIndices[slot];
        }
        if (existing == InvalidMetaTypeId) {
            if (static_cast<std::uint32_t>(g_typeClassCount) + 1U >= kMaxTypeClasses) {
                return 0U;
            }
            const std::uint16_t index = ++g_typeClassCount;
            g_denseIdByIndex[index] = typeId;
            g_typeHashKeys[slot] = typeId;
            g_typeHashIndices[slot] = index;
            return index;
        }
        slot = (slot + 1U) & kTypeClassHashMask;
    }
    return 0U;
}

std::uint16_t FindDenseIndexLocked(MetaTypeId typeId) noexcept {
    if (typeId == InvalidMetaTypeId) {
        return 0U;
    }
    std::uint32_t slot = HashTypeId(typeId);
    for (std::uint32_t probe = 0U; probe < kTypeClassHashCapacity; ++probe) {
        const MetaTypeId existing = g_typeHashKeys[slot];
        if (existing == typeId) {
            return g_typeHashIndices[slot];
        }
        if (existing == InvalidMetaTypeId) {
            return 0U;
        }
        slot = (slot + 1U) & kTypeClassHashMask;
    }
    return 0U;
}

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

void RecomputeTypeClassEulerTourLocked() noexcept {
    struct DfsFrame {
        const TypeClass* node = nullptr;
        const TypeClass* nextChild = nullptr;
    };
    DfsFrame stack[256];
    std::uint16_t timer = 0U;
    for (const TypeClass* root = g_rootTypeClasses; root != nullptr; root = root->nextSibling_) {
        std::uint32_t depth = 0U;
        if (!root->manualInterval_) {
            root->treeIn_ = ++timer;
        }
        stack[depth++] = {root, root->firstChild_};
        while (depth > 0U) {
            DfsFrame& top = stack[depth - 1U];
            if (top.nextChild != nullptr) {
                const TypeClass* child = top.nextChild;
                top.nextChild = child->nextSibling_;
                if (depth < 256U) {
                    if (!child->manualInterval_) {
                        child->treeIn_ = ++timer;
                    }
                    stack[depth++] = {child, child->firstChild_};
                }
            } else {
                if (!top.node->manualInterval_) {
                    top.node->treeOut_ = timer;
                }
                --depth;
            }
        }
    }
}

TypeClass::TypeClass(
    StringView ns,
    StringView name,
    const TypeClass* base,
    FactoryFn factory) noexcept
    : TypeClass(MakeMetaTypeId(ns, name), ns, name, base, factory) {}

TypeClass::TypeClass(
    MetaTypeId typeId,
    StringView ns,
    StringView name,
    const TypeClass* base,
    FactoryFn factory) noexcept
    : typeId_(typeId),
      namespace_(ns),
      name_(name),
      base_(base),
      factory_(factory) {
    TypeClassSpinGuard lock;
    const std::uint16_t existingIndex = FindDenseIndexLocked(typeId_);
    if (existingIndex != 0U && g_typeClassesByIndex[existingIndex] == nullptr) {
        denseIndex_ = existingIndex;
        g_typeClassesByIndex[existingIndex] = this;
    } else if (existingIndex == 0U) {
        denseIndex_ = FindOrCreateDenseIndexLocked(typeId_);
        if (denseIndex_ != 0U) {
            g_typeClassesByIndex[denseIndex_] = this;
        }
    } else {
        if (static_cast<std::uint32_t>(g_typeClassCount) + 1U < kMaxTypeClasses) {
            denseIndex_ = ++g_typeClassCount;
            g_denseIdByIndex[denseIndex_] = typeId_;
            g_typeClassesByIndex[denseIndex_] = this;
        }
    }

    if (base_ != nullptr) {
        nextSibling_ = base_->firstChild_;
        base_->firstChild_ = this;
    } else {
        nextSibling_ = g_rootTypeClasses;
        g_rootTypeClasses = this;
    }
    RecomputeTypeClassEulerTourLocked();
}

void TypeClass::AddProperty(const TypeProperty* prop) const noexcept {
    if (prop == nullptr) {
        return;
    }
    TypeClassSpinGuard lock;
    for (std::uint16_t i = 0U; i < propCount_; ++i) {
        if (props_[i] == prop || props_[i]->Name() == prop->Name()) {
            props_[i] = prop;
            return;
        }
    }
    if (propCount_ < kMaxInlineProperties) {
        props_[propCount_++] = prop;
    }
}

const TypeProperty* TypeClass::FindProperty(StringView name, bool inherit) const noexcept {
    for (const TypeClass* current = this; current != nullptr; current = inherit ? current->base_ : nullptr) {
        for (std::uint16_t i = 0U; i < current->propCount_; ++i) {
            const TypeProperty* prop = current->props_[i];
            if (prop != nullptr && prop->Name() == name) {
                return prop;
            }
        }
    }
    return nullptr;
}

const TypeProperty* TypeClass::FindPropertyById(MetaMemberId memberId, bool inherit) const noexcept {
    if (memberId == InvalidMetaMemberId) {
        return nullptr;
    }
    for (const TypeClass* current = this; current != nullptr; current = inherit ? current->base_ : nullptr) {
        for (std::uint16_t i = 0U; i < current->propCount_; ++i) {
            const TypeProperty* prop = current->props_[i];
            if (prop != nullptr && prop->Id() == memberId) {
                return prop;
            }
        }
    }
    return nullptr;
}

const TypeClass* TypeClass::FindById(MetaTypeId typeId) noexcept {
    if (typeId == InvalidMetaTypeId) {
        return nullptr;
    }
    TypeClassSpinGuard lock;
    const std::uint16_t index = FindDenseIndexLocked(typeId);
    return index < kMaxTypeClasses ? g_typeClassesByIndex[index] : nullptr;
}

const TypeClass* TypeClass::FindByDenseIndex(std::uint16_t denseIndex) noexcept {
    if (denseIndex == 0U || denseIndex >= kMaxTypeClasses) {
        return nullptr;
    }
    TypeClassSpinGuard lock;
    return g_typeClassesByIndex[denseIndex];
}

std::uint16_t TypeClass::InternDenseIndex(MetaTypeId typeId) noexcept {
    if (typeId == InvalidMetaTypeId) {
        return 0U;
    }
    TypeClassSpinGuard lock;
    return FindOrCreateDenseIndexLocked(typeId);
}

MetaTypeId TypeClass::ResolveDenseIndex(std::uint16_t denseIndex) noexcept {
    if (denseIndex == 0U || denseIndex >= kMaxTypeClasses) {
        return InvalidMetaTypeId;
    }
    TypeClassSpinGuard lock;
    return g_denseIdByIndex[denseIndex];
}

const TypeClass* Object::StaticClassType() noexcept {
    static const TypeClass s_root{
        StaticTypeId(),
        DefaultMetadataNamespaceUri(),
        StringView("Object"),
        nullptr,
        nullptr};
    return &s_root;
}

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

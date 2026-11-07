#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/MetadataId.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Aero::Base {

class Object;

template<class T> class Ref;

template<class T> class WeakRef;

class WeakRefBase;

using DestroyObjectFn = void(*)(Object* object, IAllocator* allocator) noexcept;

struct AdoptRefTag {};
inline constexpr AdoptRefTag AdoptRef{};

enum CoreTypeFlags : std::uint16_t {
    kCoreTypeNone             = 0U,
    kCoreTypeDependencyObject = 1U << 0U,
    kCoreTypeVisual           = 1U << 1U,
    kCoreTypeUIElement        = 1U << 2U,
    kCoreTypeFrameworkElement = 1U << 3U,
    kCoreTypeControl          = 1U << 4U,
    kCoreTypePanel            = 1U << 5U,
    kCoreTypeShape            = 1U << 6U,
    kCoreTypeContentElement   = 1U << 7U,
    kCoreTypeFreezable        = 1U << 8U,
};

class AERO_BASE_API Object {
public:
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    void AddRef() noexcept;
    void Release() noexcept;
    std::uint32_t UseCount() const noexcept;
    virtual MetaTypeId RuntimeType() const noexcept { return InvalidMetaTypeId; }
    // This accessor has no mutable state and intentionally does not make plain
    // Object instances report a runtime type.
    static constexpr MetaTypeId StaticTypeId() noexcept { return MakeMetaTypeId("Object"); }
    std::uint16_t CoreTypeMask() const noexcept { return coreTypeMask_; }

protected:
    Object() noexcept = default;
    virtual ~Object();
    void ExpireWeakReferences() noexcept;
    void AddCoreTypeFlag(std::uint16_t flag) noexcept {
        coreTypeMask_ = static_cast<std::uint16_t>(coreTypeMask_ | flag);
    }

private:
    enum ManagedLifetimeFlags : std::uint16_t {
        kManagedNone            = 0U,
        kManagedSystemAllocator = 1U << 0U,
        kManagedCustomAllocator = 1U << 1U,
    };

    struct CustomAllocationHeader {
        IAllocator* allocator = nullptr;
        DestroyObjectFn destroy = nullptr;
    };

    mutable std::atomic<std::uint32_t> strongCount_{0U};
    std::uint16_t coreTypeMask_ = 0U;
    std::uint16_t managedFlags_ = kManagedNone;
    mutable std::atomic<void*> control_{nullptr};

    bool TryAddStrongReference() noexcept;
    void AttachSystemManagedLifetime() noexcept;
    bool AttachManagedLifetime(IAllocator& allocator, DestroyObjectFn destroy) noexcept;

    template<class T> friend class Ref;

    template<class T> friend class WeakRef;

    template<class T, class... Args> friend Ref<T> MakeRefWithAllocator(IAllocator&, Args&&...) noexcept;

    friend class WeakRefBase;
};

// Non-template shared state for WeakRef<T>. All control-block interactions are
// implemented in the Aero.Base translation unit so the public headers only
// carry an opaque handle.
class AERO_BASE_API WeakRefBase {
public:
    constexpr WeakRefBase() noexcept = default;

    WeakRefBase(const WeakRefBase& other) noexcept;
    WeakRefBase(WeakRefBase&& other) noexcept;
    ~WeakRefBase() noexcept;
    WeakRefBase& operator=(const WeakRefBase& other) noexcept;
    WeakRefBase& operator=(WeakRefBase&& other) noexcept;

    void Reset() noexcept;
    void Swap(WeakRefBase& other) noexcept;
    bool Expired() const noexcept;
    Object* LockObject() const noexcept;
    Object* PeekObject() const noexcept;

protected:
    void Attach(Object& object) noexcept;
    bool TryAttach(Object& object) noexcept;

    void* control_ = nullptr;
};

} // namespace Aero::Base

namespace Aero {

using Object = Base::Object;

} // namespace Aero

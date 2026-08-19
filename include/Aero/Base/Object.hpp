#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/MetadataId.hpp>

#include <cstddef>

namespace Aero::Base {

class Object;

template<class T>
class Ref;

template<class T>
class WeakRef;

class WeakRefBase;

using DestroyObjectFn = void(*)(
    Object* object,
    IAllocator* allocator,
    std::size_t size,
    std::size_t alignment) noexcept;

struct AdoptRefTag {};
inline constexpr AdoptRefTag AdoptRef{};

class AERO_BASE_API Object {
public:
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    void AddRef() noexcept;
    void Release() noexcept;
    std::uint32_t UseCount() const noexcept;
    virtual MetaTypeId RuntimeType() const noexcept {
        return InvalidMetaTypeId;
    }
    // This accessor has no mutable state and intentionally does not make plain
    // Object instances report a runtime type.
    static constexpr MetaTypeId StaticTypeId() noexcept {
        return MakeMetaTypeId("Object");
    }

protected:
    Object() noexcept = default;
    virtual ~Object() = default;

private:
    void* control_ = nullptr;

    bool TryAddStrongReference() noexcept;
    bool AttachManagedLifetime(
        IAllocator& allocator,
        DestroyObjectFn destroy,
        std::size_t objectSize,
        std::size_t objectAlignment) noexcept;

    template<class T>
    friend class Ref;

    template<class T>
    friend class WeakRef;

    template<class T, class... Args>
    friend Result<Ref<T>> MakeRefWithAllocator(IAllocator&, Args&&...) noexcept;

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

protected:
    void Attach(Object& object) noexcept;

    void* control_ = nullptr;
};

} // namespace Aero::Base

namespace Aero {

using Object = Base::Object;

} // namespace Aero
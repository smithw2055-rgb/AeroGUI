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

namespace Detail {

struct ObjectControlBlock;
using DestroyObjectFn = void(*)(
    Object* object,
    IAllocator* allocator,
    std::size_t size,
    std::size_t alignment) noexcept;

AERO_BASE_API ObjectControlBlock* CreateObjectControlBlock(
    IAllocator& allocator,
    Object* object,
    std::size_t objectSize,
    std::size_t objectAlignment,
    DestroyObjectFn destroy) noexcept;

AERO_BASE_API void AddStrong(ObjectControlBlock* control) noexcept;
AERO_BASE_API bool AcquireStrong(ObjectControlBlock* control) noexcept;
AERO_BASE_API void ReleaseStrong(ObjectControlBlock* control) noexcept;
AERO_BASE_API void AddWeak(ObjectControlBlock* control) noexcept;
AERO_BASE_API void ReleaseWeak(ObjectControlBlock* control) noexcept;
AERO_BASE_API Object* GetObject(ObjectControlBlock* control) noexcept;
AERO_BASE_API std::uint32_t GetStrongCount(ObjectControlBlock* control) noexcept;

} // namespace Detail

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
    Detail::ObjectControlBlock* control_ = nullptr;

    void AttachControlBlock(Detail::ObjectControlBlock* control) noexcept;
    Detail::ObjectControlBlock* ControlBlock() const noexcept;

    template<class T>
    friend class Ref;

    template<class T>
    friend class WeakRef;

    template<class T, class... Args>
    friend Result<Ref<T>> MakeRefWithAllocator(IAllocator&, Args&&...) noexcept;
};

} // namespace Aero::Base

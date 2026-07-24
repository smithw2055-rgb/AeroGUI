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

AERO_API ObjectControlBlock* CreateObjectControlBlock(
    IAllocator& allocator,
    Object* object,
    std::size_t objectSize,
    std::size_t objectAlignment,
    DestroyObjectFn destroy) noexcept;

AERO_API void AddStrong(ObjectControlBlock* control) noexcept;
AERO_API bool TryAddStrong(ObjectControlBlock* control) noexcept;
AERO_API void ReleaseStrong(ObjectControlBlock* control) noexcept;
AERO_API void AddWeak(ObjectControlBlock* control) noexcept;
AERO_API void ReleaseWeak(ObjectControlBlock* control) noexcept;
AERO_API Object* GetObject(ObjectControlBlock* control) noexcept;
AERO_API std::uint32_t GetStrongCount(ObjectControlBlock* control) noexcept;

} // namespace Detail

class Object {
public:
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    void AddRef() noexcept;
    bool TryAddRef() noexcept;
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

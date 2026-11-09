#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/StringView.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Aero::Base {

class Object;
class Value;
template<class T> class Result;

template<class T> class Ref;

template<class T> class WeakRef;

class WeakRefBase;
class TypeClass;
class TypeProperty;

using DestroyObjectFn = void(*)(Object* object, IAllocator* allocator) noexcept;
using PropertyGetCallback = Result<Value> (*)(const Object& object, void* context) noexcept;
using PropertySetCallback = void (*)(Object& object, const Value& value, void* context) noexcept;

struct AdoptRefTag {};
inline constexpr AdoptRefTag AdoptRef{};

// Pointer-identity property descriptor with zero-allocation static thunks (Noesis-style).
class AERO_BASE_API TypeProperty {
public:
    using GetterFn = void (*)(const Object* obj, void* outValue) noexcept;
    using SetterFn = void (*)(Object* obj, const void* inValue) noexcept;

    constexpr TypeProperty(
        StringView name,
        MetaTypeId valueTypeId,
        GetterFn get,
        SetterFn set,
        PropertyGetCallback metaGet = nullptr,
        PropertySetCallback metaSet = nullptr,
        const TypeClass* valueType = nullptr,
        MetaMemberId memberId = InvalidMetaMemberId) noexcept
        : name_(name),
          memberId_(memberId),
          valueTypeId_(valueTypeId),
          valueType_(valueType),
          getter_(get),
          setter_(set),
          metaGet_(metaGet),
          metaSet_(metaSet) {}

    constexpr TypeProperty(
        StringView name,
        MetaMemberId memberId,
        MetaTypeId valueTypeId,
        GetterFn get,
        SetterFn set,
        PropertyGetCallback metaGet = nullptr,
        PropertySetCallback metaSet = nullptr,
        const TypeClass* valueType = nullptr) noexcept
        : name_(name),
          memberId_(memberId),
          valueTypeId_(valueTypeId),
          valueType_(valueType),
          getter_(get),
          setter_(set),
          metaGet_(metaGet),
          metaSet_(metaSet) {}

    constexpr StringView Name() const noexcept { return name_; }
    constexpr MetaMemberId Id() const noexcept { return memberId_; }
    constexpr MetaMemberId MemberId() const noexcept { return memberId_; }
    constexpr MetaTypeId ValueTypeId() const noexcept { return valueTypeId_; }
    constexpr const TypeClass* ValueType() const noexcept { return valueType_; }
    constexpr bool IsReadOnly() const noexcept { return setter_ == nullptr && metaSet_ == nullptr; }
    constexpr PropertyGetCallback MetaGetter() const noexcept { return metaGet_; }
    constexpr PropertySetCallback MetaSetter() const noexcept { return metaSet_; }
    void GetRaw(const Object* obj, void* out) const noexcept {
        if (getter_ != nullptr) {
            getter_(obj, out);
        }
    }
    void SetRaw(Object* obj, const void* in) const noexcept {
        if (setter_ != nullptr) {
            setter_(obj, in);
        }
    }

private:
    StringView name_;
    MetaMemberId memberId_ = InvalidMetaMemberId;
    MetaTypeId valueTypeId_ = InvalidMetaTypeId;
    const TypeClass* valueType_ = nullptr;
    GetterFn getter_ = nullptr;
    SetterFn setter_ = nullptr;
    PropertyGetCallback metaGet_ = nullptr;
    PropertySetCallback metaSet_ = nullptr;
};

struct TypePropertySlice {
    const TypeProperty* const* data = nullptr;
    std::uint32_t size = 0U;

    constexpr const TypeProperty* const* Data() const noexcept { return data; }
    constexpr std::uint32_t Size() const noexcept { return size; }
    constexpr bool Empty() const noexcept { return size == 0U; }
    constexpr const TypeProperty* operator[](std::uint32_t index) const noexcept { return data[index]; }
    constexpr const TypeProperty* const* begin() const noexcept { return data; }
    constexpr const TypeProperty* const* end() const noexcept { return data + size; }
};

// Pointer-identity type descriptor supporting O(1) Euler-interval / base-chain hierarchy checks.
class AERO_BASE_API TypeClass final {
public:
    using FactoryFn = Ref<Object> (*)() noexcept;
    static constexpr std::uint32_t kMaxInlineProperties = 48U;

    TypeClass(
        StringView ns,
        StringView name,
        const TypeClass* base,
        FactoryFn factory = nullptr) noexcept;

    TypeClass(
        MetaTypeId typeId,
        StringView ns,
        StringView name,
        const TypeClass* base,
        FactoryFn factory = nullptr) noexcept;

    constexpr MetaTypeId Id() const noexcept { return typeId_; }
    constexpr StringView Namespace() const noexcept { return namespace_; }
    constexpr StringView Name() const noexcept { return name_; }
    constexpr const TypeClass* Base() const noexcept { return base_; }
    constexpr const TypeClass* BaseType() const noexcept { return base_; }
    constexpr FactoryFn Factory() const noexcept { return factory_; }
    void SetFactory(FactoryFn factory) const noexcept { factory_ = factory; }

    constexpr std::uint16_t DenseIndex() const noexcept { return denseIndex_; }
    constexpr std::uint16_t TreeIn() const noexcept { return treeIn_; }
    constexpr std::uint16_t TreeOut() const noexcept { return treeOut_; }

    void SetEulerInterval(std::uint16_t treeIn, std::uint16_t treeOut) const noexcept {
        treeIn_ = treeIn;
        treeOut_ = treeOut;
        manualInterval_ = true;
    }

    bool IsDescendantOf(const TypeClass* target) const noexcept {
        if (this == target) {
            return true;
        }
        if (target == nullptr) {
            return false;
        }
        if (treeIn_ != 0U && target->treeIn_ != 0U && !manualInterval_ && !target->manualInterval_) {
            return treeIn_ >= target->treeIn_ && treeIn_ <= target->treeOut_;
        }
        for (const TypeClass* current = base_; current != nullptr; current = current->base_) {
            if (current == target) {
                return true;
            }
        }
        return false;
    }

    void AddProperty(const TypeProperty* prop) const noexcept;
    const TypeProperty* FindProperty(StringView name, bool inherit = true) const noexcept;
    const TypeProperty* FindPropertyById(MetaMemberId memberId, bool inherit = true) const noexcept;
    TypePropertySlice Properties() const noexcept { return {props_, propCount_}; }

    static const TypeClass* FindById(MetaTypeId typeId) noexcept;
    static const TypeClass* FindByDenseIndex(std::uint16_t denseIndex) noexcept;
    static std::uint16_t InternDenseIndex(MetaTypeId typeId) noexcept;
    static MetaTypeId ResolveDenseIndex(std::uint16_t denseIndex) noexcept;

private:
    friend void RecomputeTypeClassEulerTourLocked() noexcept;

    MetaTypeId typeId_ = InvalidMetaTypeId;
    StringView namespace_;
    StringView name_;
    const TypeClass* base_ = nullptr;
    mutable FactoryFn factory_ = nullptr;
    mutable std::uint16_t denseIndex_ = 0U;
    mutable std::uint16_t treeIn_ = 0U;
    mutable std::uint16_t treeOut_ = 0U;
    mutable std::uint16_t propCount_ = 0U;
    mutable bool manualInterval_ = false;
    mutable const TypeClass* firstChild_ = nullptr;
    mutable const TypeClass* nextSibling_ = nullptr;
    mutable const TypeProperty* props_[kMaxInlineProperties]{};
};

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

    static const TypeClass* StaticClassType() noexcept;
    virtual const TypeClass* GetClassType() const noexcept { return StaticClassType(); }

    virtual MetaTypeId RuntimeType() const noexcept {
        const TypeClass* cls = GetClassType();
        return (cls != nullptr && cls != StaticClassType()) ? cls->Id() : InvalidMetaTypeId;
    }
    // This accessor has no mutable state and intentionally does not make plain
    // Object instances report a runtime type.
    static constexpr MetaTypeId StaticTypeId() noexcept { return MakeMetaTypeId("Object"); }
    std::uint16_t CoreTypeMask() const noexcept {
        return static_cast<std::uint16_t>(coreTypeMask_ & kCoreTypeMaskBits);
    }
    bool IsDependencyObjectCore() const noexcept {
        return (coreTypeMask_ & kCoreTypeDependencyObject) != 0U;
    }
    bool IsVisualCore() const noexcept {
        return (coreTypeMask_ & kCoreTypeVisual) != 0U;
    }

protected:
    Object() noexcept = default;
    virtual ~Object();
    void ExpireWeakReferences() noexcept;
    void AddCoreTypeFlag(std::uint16_t flag) noexcept {
        coreTypeMask_ = static_cast<std::uint16_t>(coreTypeMask_ | (flag & kCoreTypeMaskBits));
    }
    std::uint16_t ContextSlot() const noexcept { return contextSlot_; }
    void SetContextSlot(std::uint16_t slot) noexcept { contextSlot_ = slot; }
    void PromoteConstructingObject() noexcept;
    struct CustomAllocationHeader {
        IAllocator* allocator = nullptr;
        DestroyObjectFn destroy = nullptr;
    };
    bool AttachManagedLifetime(IAllocator& allocator, DestroyObjectFn destroy) noexcept;

private:
    static constexpr std::uint16_t kCoreTypeMaskBits = 0x3FFFU;

    enum ManagedLifetimeFlags : std::uint16_t {
        kManagedNone            = 0U,
        kManagedSystemAllocator = 1U << 14U,
        kManagedCustomAllocator = 1U << 15U,
        kManagedLifetimeMask    = kManagedSystemAllocator | kManagedCustomAllocator,
    };

    mutable std::atomic<std::uint32_t> strongCount_{0U};
    std::uint16_t coreTypeMask_ = 0U;
    std::uint16_t contextSlot_ = 0U;
    mutable std::atomic<void*> control_{nullptr};

    bool TryAddStrongReference() noexcept;
    void AttachSystemManagedLifetime() noexcept;

    template<class T> friend class Ref;

    template<class T> friend class WeakRef;

    template<class T, class... Args> friend Ref<T> MakeRefWithAllocator(IAllocator&, Args&&...) noexcept;

    friend class WeakRefBase;
};

inline const TypeClass* RootObjectClassType() noexcept {
    return Object::StaticClassType();
}

template<class T, class = void>
struct HasStaticClassType : std::false_type {};

template<class T>
struct HasStaticClassType<T, std::void_t<decltype(T::StaticClassType())>> : std::true_type {};

template<class T, class = void>
struct ResolveBaseStaticClassType {
    static const TypeClass* Get() noexcept {
        if constexpr (std::is_base_of_v<Object, T>) {
            return Object::StaticClassType();
        } else {
            return nullptr;
        }
    }
    operator const TypeClass*() const noexcept { return Get(); }
};

template<class T>
struct ResolveBaseStaticClassType<T, std::void_t<decltype(T::StaticClassType())>> {
    static const TypeClass* Get() noexcept {
        return T::StaticClassType();
    }
    operator const TypeClass*() const noexcept { return Get(); }
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
using TypeClass = Base::TypeClass;
using TypeProperty = Base::TypeProperty;
using TypePropertySlice = Base::TypePropertySlice;

} // namespace Aero

#pragma once

// Type-erased copy/destroy/invoke for Delegate<void(Object*, TArgs&)> so
// installed UIElement.hpp / ContentElement.hpp do not compile per-TArgs
// HandlerOperations lambdas. All such Delegates share one object layout:
// inline functor storage followed by an operations pointer.

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Events/EventArgs.hpp>

#include <cstddef>

namespace Aero {
namespace {

struct ErasedDelegateOperations {
    void (*invoke)(
        const void* storage,
        Base::Object* sender,
        RoutedEventArgs& args);
    void (*copy)(void* destination, const void* source) noexcept;
    void (*destroy)(void* storage) noexcept;
    bool (*equals)(const void* left, const void* right) noexcept;
    bool multicast;
};

struct ErasedDelegate {
    alignas(void*) unsigned char storage[Base::Delegate<void()>::StorageSize];
    const ErasedDelegateOperations* operations = nullptr;
};

using RoutedDelegate = Base::Delegate<void(Base::Object*, RoutedEventArgs&)>;

static_assert(
    sizeof(ErasedDelegate) == sizeof(RoutedDelegate),
    "Erased routed-handler layout must match Delegate");
static_assert(
    alignof(ErasedDelegate) == alignof(RoutedDelegate),
    "Erased routed-handler alignment must match Delegate");

inline void CopyErasedDelegate(void* destination, const void* source) noexcept {
    auto* dest = static_cast<ErasedDelegate*>(destination);
    const auto* src = static_cast<const ErasedDelegate*>(source);
    dest->operations = src->operations;
    if (src->operations != nullptr) {
        src->operations->copy(dest->storage, src->storage);
    }
}

inline void DestroyErasedDelegate(void* value) noexcept {
    auto* delegate = static_cast<ErasedDelegate*>(value);
    if (delegate->operations != nullptr) {
        delegate->operations->destroy(delegate->storage);
        delegate->operations = nullptr;
    }
}

inline bool EqualsErasedDelegate(const void* left, const void* right) noexcept {
    const auto* lhs = static_cast<const ErasedDelegate*>(left);
    const auto* rhs = static_cast<const ErasedDelegate*>(right);
    if (lhs->operations != rhs->operations) {
        return false;
    }
    return lhs->operations == nullptr ||
        lhs->operations->equals(lhs->storage, rhs->storage);
}

inline void InvokeErasedDelegate(
    const void* value,
    Base::Object* sender,
    RoutedEventArgs& args) noexcept {
    const auto* delegate = static_cast<const ErasedDelegate*>(value);
    AERO_ASSERT(delegate->operations != nullptr);
    delegate->operations->invoke(delegate->storage, sender, args);
}

} // namespace
} // namespace Aero

#pragma once

#include <Aero/Freezable.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero {

struct FreezableState {
    struct ConsumerRecord {
        Base::WeakRef<DependencyObject> object;
        DependencyObject* unmanagedObject = nullptr;
        Meta::DependencyPropertyHandle property;
    };

    struct HandlerRecord {
        FreezableChangedHandler handler;
        bool active = false;
    };

    explicit FreezableState(Base::IAllocator& allocator) noexcept
        : consumers(&allocator), handlers(&allocator) {}

    Base::Vector<ConsumerRecord> consumers;
    Base::Vector<HandlerRecord> handlers;
    std::uint64_t revision = 0U;
    std::uint32_t notificationDepth = 0U;
    bool frozen = false;
    bool freezing = false;
};

} // namespace Aero

#pragma once

#include <Aero/Freezable.hpp>

namespace Aero {

struct DependencyObject::Access {
    using FreezableVisitor = Base::Result<void> (*)(
        void* context,
        Freezable& child) noexcept;

    static bool HasUnfreezableValueState(
        const DependencyObject& object) noexcept;
    static Base::Result<void> VisitFreezableChildren(
        DependencyObject& object,
        void* context,
        FreezableVisitor visitor) noexcept;
    static Base::Result<void> PrepareConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void CommitConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void InvalidateSubProperty(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
};

struct Freezable::Access {
    struct ConsumerRecord {
        Base::WeakRef<DependencyObject> object;
        DependencyObject* unmanagedObject = nullptr;
        Meta::DependencyPropertyHandle property;
    };

    struct HandlerRecord {
        FreezableChangedHandler handler;
        bool active = false;
    };

    explicit Access(Base::IAllocator& allocator) noexcept
        : consumers(&allocator), handlers(&allocator) {}

    Base::Vector<ConsumerRecord> consumers;
    Base::Vector<HandlerRecord> handlers;
    std::uint64_t revision = 0U;
    std::uint32_t notificationDepth = 0U;
    bool frozen = false;
    bool freezing = false;

    static Base::Result<void> AttachConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static void DetachConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static std::uint64_t Revision(const Freezable& value) noexcept;
    static bool CheckCore(Freezable& value) noexcept {
        return value.FreezeCore(true);
    }
};

} // namespace Aero

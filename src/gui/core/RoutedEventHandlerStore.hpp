#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Events.hpp>
#include <Aero/RoutedEvent.hpp>
#include "gui/core/ErasedRoutedHandler.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace Aero {

struct RoutedHandlerRecord {
    RoutedEventHandle event;
    RoutedHandlerStorage handler;
    std::uint64_t sequence = 0U;
    bool handledEventsToo = false;
};

struct RoutedEventHandlerStore {
    Base::Vector<RoutedHandlerRecord> handlers;
    std::uint64_t nextSequence = 1U;

    static RoutedEventHandlerStore* Create() noexcept {
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(RoutedEventHandlerStore),
            alignof(RoutedEventHandlerStore),
            Base::MemoryTag::Ui});
        if (memory == nullptr) return nullptr;
        return new (memory) RoutedEventHandlerStore();
    }

    static void Destroy(RoutedEventHandlerStore* store) noexcept {
        if (store == nullptr) return;
        store->~RoutedEventHandlerStore();
        Base::GetDefaultAllocator().Deallocate(
            store,
            sizeof(RoutedEventHandlerStore),
            alignof(RoutedEventHandlerStore),
            Base::MemoryTag::Ui);
    }

    void AddHandler(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType,
        bool handledEventsToo) noexcept {
        if (nextSequence == 0U) return;
        RoutedHandlerRecord record;
        record.event = event;
        record.handler = RoutedHandlerStorage(
            handler,
            size,
            alignment,
            argsType,
            &CopyErasedDelegate,
            &DestroyErasedDelegate,
            &EqualsErasedDelegate,
            &InvokeErasedDelegate);
        record.sequence = nextSequence++;
        record.handledEventsToo = handledEventsToo;
        (void)handlers.PushBack(std::move(record));
    }

    bool RemoveHandler(
        RoutedEventHandle event,
        const void* handler,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType) noexcept {
        RoutedHandlerStorage probe(
            handler,
            size,
            alignment,
            argsType,
            &CopyErasedDelegate,
            &DestroyErasedDelegate,
            &EqualsErasedDelegate,
            &InvokeErasedDelegate);
        for (std::uint32_t index = 0U; index < handlers.Size(); ++index) {
            if (handlers[index].event == event && handlers[index].handler.Equals(probe)) {
                for (std::uint32_t current = index + 1U; current < handlers.Size(); ++current) {
                    handlers[current - 1U] = std::move(handlers[current]);
                }
                handlers.PopBack();
                return true;
            }
        }
        return false;
    }

    void Invoke(
        DependencyObject* target,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept {
        const std::uint32_t count = handlers.Size();
        for (std::uint32_t index = 0U; index < count && index < handlers.Size(); ++index) {
            const RoutedHandlerRecord record = handlers[index];
            if (record.event == event && (!args.GetHandled() || record.handledEventsToo)) {
                record.handler.Invoke(target, args);
            }
        }
    }
};

} // namespace Aero

#include <Aero/ContentElement.hpp>

#include "gui/RoutedEventInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/StyleInternal.hpp"

#include <Aero/Base/Assert.hpp>

#include <utility>

namespace Aero {
namespace {

struct RoutedHandlerRecord final {
    RoutedEventHandle event;
    Aero::Detail::RoutedHandlerStorage handler;
    std::uint64_t sequence = 0U;
    bool handledEventsToo = false;
};

struct ContentElementHandlerState final {
    Base::Vector<RoutedHandlerRecord> handlers;
    std::uint64_t nextSequence = 1U;
};

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

ContentElement::ContentElement(Core::TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {}

ContentElement::~ContentElement() {
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(contentHost_ == nullptr);
    CleanupHandlers();
}

Base::Result<void> ContentElement::TryAddHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!event.IsValid() || handler.value == nullptr ||
        handler.operations == nullptr ||
        handler.operations->copy == nullptr ||
        handler.operations->destroy == nullptr ||
        handler.operations->equals == nullptr ||
        handler.operations->invoke == nullptr ||
        handler.operations->size > 4U * sizeof(void*) ||
        handler.operations->alignment > alignof(void*)) {
        return InvalidArgument(
            "Routed event handler requires a valid event and callback");
    }

    auto* state = static_cast<ContentElementHandlerState*>(handlerState_);
    if (state == nullptr) {
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(ContentElementHandlerState),
            alignof(ContentElementHandlerState),
            Base::MemoryTag::Ui});
        if (memory == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Routed event handler state allocation failed");
        }
        state = new (memory) ContentElementHandlerState();
        handlerState_ = state;
    }
    if (state->nextSequence == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler sequence space exhausted");
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::Detail::RoutedHandlerStorage(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    record.sequence = state->nextSequence++;
    record.handledEventsToo = handledEventsToo;
    return state->handlers.TryPushBack(std::move(record));
}

bool ContentElement::RemoveHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler.value == nullptr ||
        handler.operations == nullptr || handlerState_ == nullptr) {
        return false;
    }
    Aero::Detail::RoutedHandlerStorage probe(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    auto& handlers =
        static_cast<ContentElementHandlerState*>(handlerState_)->handlers;
    for (std::uint32_t index = 0U; index < handlers.Size(); ++index) {
        if (handlers[index].event == event &&
            handlers[index].handler.Equals(probe)) {
            for (std::uint32_t current = index + 1U;
                 current < handlers.Size(); ++current) {
                handlers[current - 1U] = std::move(handlers[current]);
            }
            handlers.PopBack();
            return true;
        }
    }
    return false;
}

void ContentElement::InvokeHandlers(
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    auto* state = static_cast<ContentElementHandlerState*>(handlerState_);
    if (state == nullptr) return;
    const std::uint32_t count = state->handlers.Size();
    for (std::uint32_t index = 0U;
         index < count && index < state->handlers.Size();
         ++index) {
        const RoutedHandlerRecord record = state->handlers[index];
        if (record.event == event &&
            (!args.handled || record.handledEventsToo)) {
            record.handler.Invoke(this, args);
        }
    }
}

void ContentElement::CleanupHandlers() noexcept {
    auto* state = static_cast<ContentElementHandlerState*>(handlerState_);
    if (state == nullptr) return;
    state->~ContentElementHandlerState();
    Base::GetDefaultAllocator().Deallocate(
        state,
        sizeof(ContentElementHandlerState),
        alignof(ContentElementHandlerState),
        Base::MemoryTag::Ui);
    handlerState_ = nullptr;
}

Base::Result<void> ContentElement::RaiseEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    if (eventRouter_ == nullptr && contentHost_ != nullptr) {
        eventRouter_ = Aero::Detail::ContentElementAccess::EventRouterFor(
            *contentHost_);
    }
    if (eventRouter_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ContentElement is not attached to an event router");
    }
    return static_cast<Aero::Detail::EventRouter*>(eventRouter_)
        ->RaiseEvent(*this, event, args);
}

FrameworkContentElement::FrameworkContentElement(
    Core::TypeId runtimeType) noexcept
    : ContentElement(runtimeType) {}

FrameworkContentElement::~FrameworkContentElement() = default;

Base::Result<void> FrameworkContentElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Aero::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "FrameworkContentElement Resources is already assigned");
}

} // namespace Aero

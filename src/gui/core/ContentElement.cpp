#include <Aero/FrameworkContentElement.hpp>

#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

#include <Aero/Base/Assert.hpp>

#include <utility>

namespace Aero {
namespace {

struct RoutedHandlerRecord {
    RoutedEventHandle event;
    Aero::RoutedHandlerStorage handler;
    std::uint64_t sequence = 0U;
    bool handledEventsToo = false;
};

struct ContentElementHandlerState {
    Base::Vector<RoutedHandlerRecord> handlers;
    std::uint64_t nextSequence = 1U;
};

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

ContentElement::ContentElement(Meta::TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {}

ContentElement::~ContentElement() {
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(contentHost_ == nullptr);
    CleanupHandlers();
}

Base::Result<void> ContentElement::AddHandlerCore(
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

    auto* state = static_cast<ContentElementHandlerState*>(routedHandlers_);
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
        routedHandlers_ = state;
    }
    if (state->nextSequence == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler sequence space exhausted");
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::RoutedHandlerStorage(
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
    return state->handlers.PushBack(std::move(record));
}

bool ContentElement::RemoveHandlerCore(
    RoutedEventHandle event,
    const HandlerDescriptor& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler.value == nullptr ||
        handler.operations == nullptr || routedHandlers_ == nullptr) {
        return false;
    }
    Aero::RoutedHandlerStorage probe(
        handler.value,
        handler.operations->size,
        handler.operations->alignment,
        handler.argsType,
        handler.operations->copy,
        handler.operations->destroy,
        handler.operations->equals,
        handler.operations->invoke);
    auto& handlers =
        static_cast<ContentElementHandlerState*>(routedHandlers_)->handlers;
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
    auto* state = static_cast<ContentElementHandlerState*>(routedHandlers_);
    if (state == nullptr) return;
    const std::uint32_t count = state->handlers.Size();
    for (std::uint32_t index = 0U;
         index < count && index < state->handlers.Size();
         ++index) {
        const RoutedHandlerRecord record = state->handlers[index];
        if (record.event == event &&
            (!args.GetHandled() || record.handledEventsToo)) {
            record.handler.Invoke(this, args);
        }
    }
}

void ContentElement::CleanupHandlers() noexcept {
    auto* state = static_cast<ContentElementHandlerState*>(routedHandlers_);
    if (state == nullptr) return;
    state->~ContentElementHandlerState();
    Base::GetDefaultAllocator().Deallocate(
        state,
        sizeof(ContentElementHandlerState),
        alignof(ContentElementHandlerState),
        Base::MemoryTag::Ui);
    routedHandlers_ = nullptr;
}

void ContentElement::RaiseEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    if (eventRouter_ == nullptr && contentHost_ != nullptr) {
        eventRouter_ = Aero::Core::GetFacet<::Aero::EventRouter>(
            *contentHost_);
    }
    if (eventRouter_ == nullptr) {
        return;
    }
    static_cast<void>(
        static_cast<Aero::EventRouter*>(eventRouter_)
            ->RaiseEvent(*this, event, args));
}

FrameworkContentElement::FrameworkContentElement(
    Meta::TypeId runtimeType) noexcept
    : ContentElement(runtimeType) {}

FrameworkContentElement::~FrameworkContentElement() = default;

} // namespace Aero

namespace Aero::Core {

void VisualFacet::Attach(
    ContentElement& element,
    DependencyObject* logicalParent,
    UIElement* contentHost,
    EventRouter* eventRouter) noexcept {
    element.logicalParent_ = logicalParent;
    element.contentHost_ = contentHost;
    element.eventRouter_ = eventRouter;
}

void VisualFacet::Detach(ContentElement& element) noexcept {
    element.logicalParent_ = nullptr;
    element.contentHost_ = nullptr;
    element.eventRouter_ = nullptr;
}

DependencyObject* VisualFacet::Parent(
    const ContentElement& element) noexcept {
    return element.logicalParent_;
}

UIElement* VisualFacet::ContentHost(
    const ContentElement& element) noexcept {
    return element.contentHost_;
}

std::uint32_t VisualFacet::LogicalChildrenCount(
    const FrameworkContentElement& element) noexcept {
    return element.GetLogicalChildrenCount();
}

DependencyObject* VisualFacet::LogicalChild(
    const FrameworkContentElement& element,
    std::uint32_t index) noexcept {
    return element.GetLogicalChild(index);
}

void InputEventFacet::InvokeContentHandlers(
    Aero::ContentElement& element,
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    element.InvokeHandlers(event, args);
}

} // namespace Aero::Core

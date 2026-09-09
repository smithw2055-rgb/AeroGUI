#include <Aero/ContentElement.hpp>

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/internal/ErasedRoutedHandler.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"
#include <Aero/Meta.hpp>
#include "gui/meta/ElementsFill.hpp"

#include <Aero/Base/Assert.hpp>

#include <new>
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

} // namespace

ContentElement::ContentElement(Meta::TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {}

ContentElement::~ContentElement() {
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(contentHost_ == nullptr);
    CleanupHandlers();
}

void ContentElement::AddHandlerErased(
    RoutedEventHandle event,
    const void* handler,
    std::size_t size,
    std::size_t alignment,
    Meta::TypeId argsType,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!event.IsValid() || handler == nullptr ||
        size > 4U * sizeof(void*) ||
        alignment > alignof(void*)) {
        return;
    }

    auto* state = static_cast<ContentElementHandlerState*>(routedHandlers_);
    if (state == nullptr) {
        Base::IAllocator& allocator = Base::GetDefaultAllocator();
        void* memory = allocator.Allocate({
            sizeof(ContentElementHandlerState),
            alignof(ContentElementHandlerState),
            Base::MemoryTag::Ui});
        if (memory == nullptr) {
            return;
        }
        state = new (memory) ContentElementHandlerState();
        routedHandlers_ = state;
    }
    if (state->nextSequence == 0U) {
        return;
    }

    RoutedHandlerRecord record;
    record.event = event;
    record.handler = Aero::RoutedHandlerStorage(
        handler,
        size,
        alignment,
        argsType,
        &CopyErasedDelegate,
        &DestroyErasedDelegate,
        &EqualsErasedDelegate,
        &InvokeErasedDelegate);
    record.sequence = state->nextSequence++;
    record.handledEventsToo = handledEventsToo;
    (void)state->handlers.PushBack(std::move(record));
}

bool ContentElement::RemoveHandlerErased(
    RoutedEventHandle event,
    const void* handler,
    std::size_t size,
    std::size_t alignment,
    Meta::TypeId argsType) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access || !event.IsValid() || handler == nullptr ||
        routedHandlers_ == nullptr) {
        return false;
    }
    Aero::RoutedHandlerStorage probe(
        handler,
        size,
        alignment,
        argsType,
        &CopyErasedDelegate,
        &DestroyErasedDelegate,
        &EqualsErasedDelegate,
        &InvokeErasedDelegate);
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
        eventRouter_ = AeroGuiInternal::EventRouterOf(*contentHost_);
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

} // namespace Aero

namespace Aero {

void AeroGuiInternal::Attach(
    ContentElement& element,
    DependencyObject* logicalParent,
    UIElement* contentHost,
    EventRouter* eventRouter) noexcept {
    AERO_GET_FIELD(element, ContentElement_logicalParent) = logicalParent;
    AERO_GET_FIELD(element, ContentElement_contentHost) = contentHost;
    AERO_GET_FIELD(element, ContentElement_eventRouter) = eventRouter;
}

void AeroGuiInternal::Detach(ContentElement& element) noexcept {
    AERO_GET_FIELD(element, ContentElement_logicalParent) = nullptr;
    AERO_GET_FIELD(element, ContentElement_contentHost) = nullptr;
    AERO_GET_FIELD(element, ContentElement_eventRouter) = nullptr;
}

DependencyObject* AeroGuiInternal::Parent(
    const ContentElement& element) noexcept {
    return AERO_GET_FIELD(element, ContentElement_logicalParent);
}

UIElement* AeroGuiInternal::ContentHost(
    const ContentElement& element) noexcept {
    return AERO_GET_FIELD(element, ContentElement_contentHost);
}

std::uint32_t AeroGuiInternal::LogicalChildrenCount(
    const FrameworkContentElement& element) noexcept {
    return AERO_CALL_METHOD0(element, FCE_GetLogicalChildrenCount);
}

DependencyObject* AeroGuiInternal::LogicalChild(
    const FrameworkContentElement& element,
    std::uint32_t index) noexcept {
    return AERO_CALL_METHOD(element, FCE_GetLogicalChild, index);
}

void AeroGuiInternal::InvokeContentHandlers(
    Aero::ContentElement& element,
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    AERO_CALL_METHOD(element, ContentElement_InvokeHandlers, event, args);
}

} // namespace Aero

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Media::Animation::Model;
Base::Result<void> FillContentElementMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Register<ContentElement>(context, TypeFlags::Abstract);
    return {};
}
} // namespace Aero::Meta

#include <Aero/ContentElement.hpp>
#include "gui/media/AnimationModel.hpp"

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/ErasedRoutedHandler.hpp"
#include <Aero/Meta.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"

#include <Aero/Base/Assert.hpp>

#include <new>
#include <utility>

#include "gui/core/RoutedEventHandlerStore.hpp"

namespace Aero {

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

    auto* store = static_cast<RoutedEventHandlerStore*>(routedHandlers_);
    if (store == nullptr) {
        store = RoutedEventHandlerStore::Create();
        if (store == nullptr) return;
        routedHandlers_ = store;
    }
    store->AddHandler(event, handler, size, alignment, argsType, handledEventsToo);
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
    return static_cast<RoutedEventHandlerStore*>(routedHandlers_)->RemoveHandler(
        event, handler, size, alignment, argsType);
}

void ContentElement::InvokeHandlers(
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    auto* store = static_cast<RoutedEventHandlerStore*>(routedHandlers_);
    if (store == nullptr) return;
    store->Invoke(this, event, args);
}

void ContentElement::CleanupHandlers() noexcept {
    auto* store = static_cast<RoutedEventHandlerStore*>(routedHandlers_);
    if (store == nullptr) return;
    RoutedEventHandlerStore::Destroy(store);
    routedHandlers_ = nullptr;
}

void ContentElement::RaiseEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    if (eventRouter_ == nullptr && contentHost_ != nullptr) {
        eventRouter_ = ElementTree::EventsOf(*contentHost_);
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

void ContentElement::Attach(
    DependencyObject* logicalParent,
    UIElement* contentHost,
    EventRouter* eventRouter) noexcept {
    logicalParent_ = logicalParent;
    contentHost_ = contentHost;
    eventRouter_ = eventRouter;
}

void ContentElement::Detach() noexcept {
    logicalParent_ = nullptr;
    contentHost_ = nullptr;
    eventRouter_ = nullptr;
}


AERO_DESCRIBE(ContentElement) {
    using namespace Aero::Meta;
    Register<ContentElement>(context, TypeFlags::Abstract);
}

} // namespace Aero

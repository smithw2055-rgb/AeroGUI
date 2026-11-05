#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"

#include <Aero/Base/Assert.hpp>

namespace Aero::Meta {
namespace {

thread_local ObjectFactoryState* activeObjectFactory = nullptr;

struct GlobalDefaultObjectFactory {
    Dispatcher dispatcher;
    TypeRegistry types;
    BehaviorTable behaviors{types};
    DependencyPropertyRegistry properties{types, behaviors};
    bool ready = false;

    GlobalDefaultObjectFactory() noexcept {
        ready = types.Freeze() &&
            behaviors.Freeze() &&
            properties.Freeze();
    }
};

GlobalDefaultObjectFactory& GetGlobalDefaultObjectFactory() noexcept {
    static GlobalDefaultObjectFactory fallback;
    AERO_ASSERT(fallback.ready);
    return fallback;
}

} // namespace

ObjectFactoryState CurrentObjectFactory() noexcept {
    if (activeObjectFactory != nullptr &&
        activeObjectFactory->dispatcher != nullptr &&
        activeObjectFactory->dependencyProperties != nullptr) {
        return *activeObjectFactory;
    }
    GlobalDefaultObjectFactory& fallback = GetGlobalDefaultObjectFactory();
    if (activeObjectFactory != nullptr) {
        ObjectFactoryState state = *activeObjectFactory;
        if (state.dispatcher == nullptr) {
            state.dispatcher = &fallback.dispatcher;
        }
        if (state.dependencyProperties == nullptr) {
            state.dependencyProperties = &fallback.properties;
        }
        return state;
    }
    return {&fallback.dispatcher, &fallback.properties, nullptr};
}

bool HasObjectFactory() noexcept {
    return activeObjectFactory != nullptr;
}

Base::Result<Value> TryEncodeValue(
    TypeId type,
    const void* source) noexcept {
    if (activeObjectFactory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Runtime value creation requires an active ObjectFactoryScope");
    }
    if (activeObjectFactory->metadata != nullptr) {
        return activeObjectFactory->metadata->TryCreateValue(
            type, source);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Object factory do not provide metadata value semantics");
}

ObjectFactoryScope::ObjectFactoryScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties) noexcept
    : state_{&dispatcher, &properties, nullptr},
      previous_(activeObjectFactory),
      ownerThread_(CurrentDispatcherThreadToken()) {
    activeObjectFactory = &state_;
}

ObjectFactoryScope::ObjectFactoryScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    Meta::Registry& runtime) noexcept
    : state_{&dispatcher, &properties, &runtime},
      previous_(activeObjectFactory),
      ownerThread_(CurrentDispatcherThreadToken()) {
    activeObjectFactory = &state_;
}

ObjectFactoryScope::ObjectFactoryScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    Meta::Registry* runtime) noexcept
    : state_{&dispatcher, &properties, runtime},
      previous_(activeObjectFactory),
      ownerThread_(CurrentDispatcherThreadToken()) {
    activeObjectFactory = &state_;
}

ObjectFactoryScope::~ObjectFactoryScope() {
    AERO_ASSERT(ownerThread_ == CurrentDispatcherThreadToken());
    AERO_ASSERT(activeObjectFactory == &state_);
    activeObjectFactory = previous_;
}

} // namespace Aero::Meta

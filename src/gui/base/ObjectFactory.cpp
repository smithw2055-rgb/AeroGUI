#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"

#include <Aero/Base/Assert.hpp>

namespace Aero::Meta {
namespace {

thread_local ObjectFactoryState* activeObjectFactory = nullptr;

struct FallbackObjectFactory {
    Dispatcher dispatcher;
    TypeRegistry types;
    BehaviorTable behaviors{types};
    DependencyPropertyRegistry properties{types, behaviors};
    bool ready = false;

    FallbackObjectFactory() noexcept {
        ready = types.Freeze() &&
            behaviors.Freeze() &&
            properties.Freeze();
    }
};

FallbackObjectFactory& GetFallbackObjectFactory() noexcept {
    thread_local FallbackObjectFactory fallback;
    AERO_ASSERT(fallback.ready);
    return fallback;
}

} // namespace

ObjectFactoryState CurrentObjectFactory() noexcept {
    if (activeObjectFactory != nullptr) {
        return *activeObjectFactory;
    }
    FallbackObjectFactory& fallback = GetFallbackObjectFactory();
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

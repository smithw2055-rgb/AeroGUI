#include "ObjectServices.hpp"

#include <Aero/Base/Assert.hpp>
#include "metadata/MetadataBehaviorRegistrationStore.hpp"
#include <Aero/Meta/MetadataRuntime.hpp>

namespace Aero::Core {
namespace {

thread_local ObjectServices* CurrentObjectServices = nullptr;

struct FallbackObjectServices final {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore behaviors{types};
    DependencyPropertyRegistry properties{types, behaviors};
    bool ready = false;

    FallbackObjectServices() noexcept {
        ready = types.Freeze() &&
            behaviors.Freeze() &&
            properties.Freeze();
    }
};

FallbackObjectServices& GetFallbackObjectServices() noexcept {
    thread_local FallbackObjectServices fallback;
    AERO_ASSERT(fallback.ready);
    return fallback;
}

} // namespace

ObjectServices GetCurrentObjectServices() noexcept {
    if (CurrentObjectServices != nullptr) {
        return *CurrentObjectServices;
    }
    FallbackObjectServices& fallback = GetFallbackObjectServices();
    return {&fallback.dispatcher, &fallback.properties, nullptr};
}

bool HasCurrentObjectServices() noexcept {
    return CurrentObjectServices != nullptr;
}

Base::Result<Value> TryCreateRuntimeValue(
    TypeId type,
    const void* source) noexcept {
    if (CurrentObjectServices == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Runtime value creation requires an active ObjectServicesScope");
    }
    if (CurrentObjectServices->metadataRuntime != nullptr) {
        return CurrentObjectServices->metadataRuntime->TryCreateValue(
            type, source);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Object services do not provide metadata value semantics");
}

ObjectServicesScope::ObjectServicesScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties) noexcept
    : services_{&dispatcher, &properties, nullptr},
      previous_(CurrentObjectServices),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentObjectServices = &services_;
}

ObjectServicesScope::ObjectServicesScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    MetadataRuntime& runtime) noexcept
    : services_{&dispatcher, &properties, &runtime},
      previous_(CurrentObjectServices),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentObjectServices = &services_;
}

ObjectServicesScope::ObjectServicesScope(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& properties,
    MetadataRuntime* runtime) noexcept
    : services_{&dispatcher, &properties, runtime},
      previous_(CurrentObjectServices),
      ownerThread_(CurrentDispatcherThreadToken()) {
    CurrentObjectServices = &services_;
}

ObjectServicesScope::~ObjectServicesScope() {
    AERO_ASSERT(ownerThread_ == CurrentDispatcherThreadToken());
    AERO_ASSERT(CurrentObjectServices == &services_);
    CurrentObjectServices = previous_;
}

} // namespace Aero::Core

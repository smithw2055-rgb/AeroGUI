#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Core {

class MetadataRuntime;

struct ObjectServices final {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    MetadataRuntime* metadataRuntime = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

AERO_API ObjectServices GetCurrentObjectServices() noexcept;
AERO_API bool HasCurrentObjectServices() noexcept;

AERO_API Base::Result<Value> TryCreateRuntimeValue(
    TypeId type,
    const void* source) noexcept;

class AERO_API ObjectServicesScope final {
public:
    ObjectServicesScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
    ObjectServicesScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime& runtime) noexcept;
    ObjectServicesScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime* runtime) noexcept;
    ~ObjectServicesScope();

    ObjectServicesScope(const ObjectServicesScope&) = delete;
    ObjectServicesScope& operator=(const ObjectServicesScope&) = delete;
    ObjectServicesScope(ObjectServicesScope&&) = delete;
    ObjectServicesScope& operator=(ObjectServicesScope&&) = delete;

private:
    ObjectServices services_;
    ObjectServices* previous_ = nullptr;
    DispatcherThreadToken ownerThread_ = 0U;
};

} // namespace Aero::Core

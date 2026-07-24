#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/MetadataRegistrationValues.hpp>

namespace Aero::Core {

class RoutedEventRegistry;
class MetadataRuntime;
struct RoutedEventHandle;
enum class RoutingStrategy : std::uint8_t;

struct PresentationContext final {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    MetadataValueRegistrationStore* valueRegistrations = nullptr;
    MetadataRuntime* metadataRuntime = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

AERO_API PresentationContext
GetCurrentPresentationContext() noexcept;

AERO_API Base::Result<Value> TryCreatePresentationValue(
    TypeId type,
    const void* source) noexcept;

class AERO_API PresentationContextScope final {
public:
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataValueRegistrationStore& values) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime& runtime) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime* runtime) noexcept;
    ~PresentationContextScope();

    PresentationContextScope(const PresentationContextScope&) = delete;
    PresentationContextScope& operator=(const PresentationContextScope&) = delete;
    PresentationContextScope(PresentationContextScope&&) = delete;
    PresentationContextScope& operator=(PresentationContextScope&&) = delete;

private:
    PresentationContext context_;
    PresentationContext* previous_ = nullptr;
    DispatcherThreadToken ownerThread_ = 0U;
};

class MetaRegistrationContext final {
public:
    MetaRegistrationContext(
        TypeRegistry& typeRegistry,
        MetadataBehaviorRegistrationStore& behaviors,
        MetadataValueRegistrationStore& values,
        DependencyPropertyRegistry& properties,
        RoutedEventRegistry* events = nullptr) noexcept
        : types_(&typeRegistry),
          behaviorRegistrations_(&behaviors),
          valueRegistrations_(&values),
          dependencyProperties_(&properties),
          routedEvents_(events) {}

    MetadataRegistrationTypes Types() noexcept {
        return MetadataRegistrationTypes(*types_, *behaviorRegistrations_);
    }

    MetadataRegistrationValues Values() noexcept {
        return MetadataRegistrationValues(*valueRegistrations_);
    }

    MetadataRegistrationValues Values() const noexcept {
        return MetadataRegistrationValues(
            static_cast<const MetadataValueRegistrationStore&>(
                *valueRegistrations_));
    }

    const TypeRegistry& TypeView() const noexcept { return *types_; }
    MetadataValueRegistrationStore& ValueRegistrations() noexcept {
        return *valueRegistrations_;
    }
    DependencyPropertyRegistry& DependencyProperties() noexcept {
        return *dependencyProperties_;
    }
    RoutedEventRegistry* RoutedEvents() const noexcept {
        return routedEvents_;
    }

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    MetadataValueRegistrationStore* valueRegistrations_ = nullptr;
    DependencyPropertyRegistry* dependencyProperties_ = nullptr;
    RoutedEventRegistry* routedEvents_ = nullptr;
};


// Registers the complete built-in presentation schema through the typed
// Fluent metadata DSL. The function leaves all stores mutable for host modules.
AERO_API Base::Result<void>
TryRegisterPresentationMetadata(
    MetaRegistrationContext& context) noexcept;

} // namespace Aero::Core

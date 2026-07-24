#pragma once

#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>
#include <Aero/Core/Events/RoutedEventCatalog.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Core {

class MetaRegistrationContext final {
public:
    MetaRegistrationContext(
        TypeRegistry& typeRegistry,
        MetadataBehaviorRegistrationStore& behaviors,
        MetadataValueRegistrationStore& values,
        DependencyPropertyRegistry& properties,
        RoutedEventCatalog* events = nullptr) noexcept
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
    RoutedEventCatalog* RoutedEvents() const noexcept {
        return routedEvents_;
    }

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    MetadataValueRegistrationStore* valueRegistrations_ = nullptr;
    DependencyPropertyRegistry* dependencyProperties_ = nullptr;
    RoutedEventCatalog* routedEvents_ = nullptr;
};

} // namespace Aero::Core

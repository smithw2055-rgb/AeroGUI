#pragma once

#include "RoutedEventCatalog.hpp"
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Core::Detail {

struct MetadataContextState final {
    TypeRegistry* types = nullptr;
    MetadataBehaviorRegistrationStore* behaviors = nullptr;
    MetadataValueRegistrationStore* values = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    RoutedEventCatalog* events = nullptr;
};

} // namespace Aero::Core::Detail

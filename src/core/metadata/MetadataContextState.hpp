#pragma once

#include "RoutedEventCatalog.hpp"
#include "MetadataBehaviorRegistrationStore.hpp"
#include <Aero/Meta/MetadataRegistrationValues.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Core::Detail {

struct MetadataContextState final {
    TypeRegistry* types = nullptr;
    MetadataBehaviorRegistrationStore* behaviors = nullptr;
    MetadataValueRegistrationStore* values = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    RoutedEventCatalog* events = nullptr;
};

} // namespace Aero::Core::Detail

#pragma once

#include <Aero/Core/Metadata/MetadataDomain.hpp>

namespace Aero::Core::Detail {

class MetadataDomainAccess final {
public:
    static DependencyPropertyRegistry& DependencyProperties(
        MetadataDomain& domain) noexcept {
        return domain.DependencyProperties();
    }

    static RoutedEventCatalog& RoutedEvents(
        MetadataDomain& domain) noexcept {
        return domain.RoutedEvents();
    }
};

} // namespace Aero::Core::Detail

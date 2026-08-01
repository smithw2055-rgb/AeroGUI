#pragma once

#include <Aero/Meta/MetadataDomain.hpp>

namespace Aero::Core::Detail {

class MetadataDomainAccess final {
public:
    static DependencyPropertyRegistry& DependencyProperties(
        MetadataDomain& domain) noexcept {
        return domain.DependencyProperties();
    }

    static void* RoutedEventState(
        MetadataDomain& domain) noexcept {
        return domain.RoutedEventState();
    }
};

} // namespace Aero::Core::Detail

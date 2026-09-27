#pragma once

#include <Aero/DependencyObject.hpp>

namespace Aero {

// Implementation-only accessors. Installed DependencyObject does not publish
// the runtime property registry to SDK consumers.
class DependencyObjectAccess {
public:
    static Meta::DependencyPropertyRegistry& PropertyRegistry(
        const DependencyObject& object) noexcept {
        return object.PropertyRegistry();
    }

    static Meta::DependencyPropertyRegistry& PropertyRegistry(
        const DependencyObject* object) noexcept {
        return object->PropertyRegistry();
    }

    static bool HasPropertyRegistry(
        const DependencyObject& object) noexcept {
        return object.HasPropertyRegistry();
    }

    static bool HasPropertyRegistry(
        const DependencyObject* object) noexcept {
        return object->HasPropertyRegistry();
    }
};

} // namespace Aero

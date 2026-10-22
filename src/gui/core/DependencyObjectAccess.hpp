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

    static const PropertyStore* Store(
        const DependencyObject& object) noexcept {
        return object.Store();
    }

    static Base::Result<void> VisitFreezableChildren(
        DependencyObject& object,
        void* context,
        Base::Result<void> (*visitor)(void* context, Freezable& child) noexcept) noexcept {
        return object.VisitFreezableChildren(context, visitor);
    }
};

} // namespace Aero

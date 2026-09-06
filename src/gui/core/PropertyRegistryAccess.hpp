#pragma once

#include <Aero/DependencyObject.hpp>
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

namespace Aero {

inline Meta::DependencyPropertyRegistry& PropertyRegistry(
    const DependencyObject& object) noexcept {
    return AeroGuiInternal::PropertyRegistry(object);
}

inline Meta::DependencyPropertyRegistry& PropertyRegistry(
    const DependencyObject* object) noexcept {
    return AeroGuiInternal::PropertyRegistry(object);
}

template<class T>
inline Meta::DependencyPropertyRegistry& PropertyRegistry(
    const Base::Ref<T>& ref) noexcept {
    return PropertyRegistry(ref.Get());
}

} // namespace Aero

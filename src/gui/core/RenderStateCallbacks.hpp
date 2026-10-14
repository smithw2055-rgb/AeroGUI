#pragma once

// Shared DP changed/validate callbacks for builtin metadata describes.
// Single home (defined in core/UIElement.cpp): several translation units
// take their address, so they stay here instead of being copied per type.

#include <Aero/Base/Result.hpp>

namespace Aero { class DependencyObject; }
namespace Aero { class DependencyPropertyChangedEventArgs; }

namespace Aero {

bool ValidateUnitDouble(
    const double& value) noexcept;
void OnRenderStateChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept;
void OnOpacityMaskChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept;
void OnRenderTransformChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept;
void OnEffectChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept;

} // namespace Aero

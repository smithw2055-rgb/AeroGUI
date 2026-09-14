#pragma once

// Shared DP changed/validate callbacks for builtin metadata Fill functions.
// Single home (defined in core/UIElement.cpp): UIElement/Media fills in
// different translation units share them. Previously duplicated via
// meta/Support.inl; do not re-add copies there.

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

#pragma once

// Shared diagnostics helper for the trigger engine. Promoted from an
// file-local helper in styles/Style.cpp so the per-type trigger
// implementations under triggers/ can share it.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>

namespace Aero {

Base::Result<void> InvalidStyle(const char* message) noexcept;

// Returns true when a setter value is a deferred Binding that must be applied
// at load time rather than as a trigger-driven value. Promoted from a
// file-local helper in styles/Style.cpp so StyleEngine and TriggerEngine can
// share it.
bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept;

} // namespace Aero

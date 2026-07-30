#pragma once

#include <Aero/Presentation/Binding.hpp>

namespace Aero::Data {

// Public binding authoring surface. Runtime descriptors, handles and the
// BindingManager remain in the implementation namespace and are intentionally
// not projected into Aero::Data.
using Binding = ::Aero::Presentation::BindingSpec;
using BindingMode = ::Aero::Presentation::BindingMode;
using RelativeSourceMode =
    ::Aero::Presentation::BindingRelativeSource;
using UpdateSourceTrigger = ::Aero::Core::UpdateSourceTrigger;

} // namespace Aero::Data

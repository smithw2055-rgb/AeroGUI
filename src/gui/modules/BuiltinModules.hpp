#pragma once

#include <Aero/Module.hpp>

namespace Aero::Meta { class Registry; class Registration; }


namespace Aero {

// Registers the platform-neutral WPF/XAML object model. Product-specific
// modules are composed by ModuleSet before the Markup layer is sealed.
AERO_API Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept;
AERO_API Base::Result<void> RegisterBuiltInMarkupModule(
    ::Aero::Meta::Registry& domain) noexcept;

} // namespace Aero

#pragma once

#include <Aero/Module.hpp>

namespace Aero::Meta { class Registry; class Registration; }


namespace Aero {

// Registers the platform-neutral WPF/XAML object model. Product-specific
// modules are composed by ModuleSet before the Markup layer is sealed.
Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept;
Base::Result<void> RegisterBuiltInMarkupModule(
    ::Aero::Meta::Registry& domain) noexcept;

} // namespace Aero

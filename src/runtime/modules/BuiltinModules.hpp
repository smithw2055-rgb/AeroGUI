#pragma once

#include <Aero/Module.hpp>

namespace Aero::Meta { class Registry; class Registration; }


namespace Aero {

AERO_API Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept;

} // namespace Aero

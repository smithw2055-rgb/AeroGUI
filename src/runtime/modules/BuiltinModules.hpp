#pragma once

#include <Aero/Module.hpp>

namespace Aero::Core {
class MetaRegistry;
}

namespace Aero {

AERO_API Base::Result<void> RegisterBuiltInUiModules(
    Core::MetaRegistry& domain) noexcept;

} // namespace Aero

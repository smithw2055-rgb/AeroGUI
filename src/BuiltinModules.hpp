#pragma once

#include <Aero/Module.hpp>

namespace Aero::Core {
class MetadataDomain;
}

namespace Aero {

AERO_API Base::Result<void> RegisterBuiltInUiModules(
    Core::MetadataDomain& domain) noexcept;

} // namespace Aero

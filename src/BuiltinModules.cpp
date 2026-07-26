#include <Aero/BuiltinModules.hpp>

#include <Aero/Controls/Metadata.hpp>

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    Core::MetadataDomain& domain) noexcept {
    return Controls::TryRegisterBuiltInUiMetadata(domain);
}

} // namespace Aero

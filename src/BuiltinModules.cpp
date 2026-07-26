#include <Aero/BuiltinModules.hpp>

#include <Aero/Controls/Metadata.hpp>
#include <Aero/Markup/Metadata.hpp>

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    Core::MetadataDomain& domain) noexcept {
    Base::Result<void> registered =
        Controls::TryRegisterBuiltInUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::TryRegisterMarkupMetadata(domain);
}

} // namespace Aero

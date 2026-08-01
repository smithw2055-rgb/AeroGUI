#include "BuiltinModules.hpp"

#include "app/Metadata.hpp"
#include "controls/Metadata.hpp"
#include "gui/MetaInternals.hpp"
#include <Aero/Markup/Schema.hpp>

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    Core::MetaRegistry& domain) noexcept {
    Base::Result<void> registered =
        Core::TryRegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::Detail::TryRegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = App::TryRegisterAppMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Controls::TryRegisterControlsMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::TryRegisterMarkupMetadata(domain);
}

} // namespace Aero

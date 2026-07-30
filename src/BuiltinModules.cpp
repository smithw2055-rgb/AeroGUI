#include "BuiltinModules.hpp"

#include <Aero/App/Metadata.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Markup/Schema.hpp>
#include <Aero/Presentation/Metadata.hpp>

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    Core::MetadataDomain& domain) noexcept {
    Base::Result<void> registered =
        Core::TryRegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Presentation::TryRegisterPresentationMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = App::TryRegisterAppMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Controls::TryRegisterControlsMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::TryRegisterMarkupMetadata(domain);
}

} // namespace Aero

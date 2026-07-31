#include "BuiltinModules.hpp"

#include <Aero/App/Metadata.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Markup/Schema.hpp>
#include <Aero/Detail/UiMetadata.hpp>

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    Core::MetadataDomain& domain) noexcept {
    Base::Result<void> registered =
        Core::TryRegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::Detail::TryRegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    // Window derives from ContentControl, so the control descriptors must be
    // available before the App module publishes Application and Window.
    registered = Controls::TryRegisterControlsMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = App::TryRegisterAppMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::TryRegisterMarkupMetadata(domain);
}

} // namespace Aero

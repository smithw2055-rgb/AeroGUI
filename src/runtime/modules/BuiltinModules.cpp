#include "BuiltinModules.hpp"

#include "app/Metadata.hpp"
#include "controls/Metadata.hpp"
#include "gui/MetadataInternal.hpp"
#include "markup/MarkupInternal.hpp"

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
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

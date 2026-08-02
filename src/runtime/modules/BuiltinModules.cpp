#include "BuiltinModules.hpp"

#include "app/Metadata.hpp"
#include "controls/Metadata.hpp"
#include "gui/MetadataInternal.hpp"
#include "markup/MarkupInternal.hpp"

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<void> registered =
        Meta::RegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::Internal::RegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = App::RegisterAppMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Controls::RegisterControlsMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::RegisterMarkupMetadata(domain);
}

} // namespace Aero

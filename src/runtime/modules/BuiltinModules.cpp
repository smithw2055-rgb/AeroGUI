#include "BuiltinModules.hpp"

#include "app/Metadata.hpp"
#include "controls/Metadata.hpp"
#include "gui/GuiPrivate.hpp"
#include "markup/MarkupPrivate.hpp"

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<void> registered =
        Meta::RegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::GuiPrivate::Detail::RegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = App::RegisterAppMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Controls::RegisterControlsMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Markup::RegisterMarkupMetadata(domain);
}

} // namespace Aero

#include "BuiltinModules.hpp"

#include "gui/controls/Metadata.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<void> registered =
        Meta::RegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::RegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Controls::RegisterControlsMetadata(domain);
}

Base::Result<void> RegisterBuiltInMarkupModule(
    ::Aero::Meta::Registry& domain) noexcept {
    return Markup::RegisterMarkupMetadata(domain);
}

} // namespace Aero

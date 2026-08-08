#include "BuiltinModules.hpp"

#include "controls/Metadata.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "controls/ControlInternal.hpp"
#include "controls/ItemsInternal.hpp"
#include "controls/TemplateInternal.hpp"
#include "markup/MarkupInternal.hpp"
#include "markup/MarkupWriterInternal.hpp"

namespace Aero {

Base::Result<void> RegisterBuiltInUiModules(
    ::Aero::Meta::Registry& domain) noexcept {
    Base::Result<void> registered =
        Meta::RegisterCoreMetadata(domain);
    if (!registered) return registered.GetStatus();
    registered = Aero::GuiPrivate::Detail::RegisterUiMetadata(domain);
    if (!registered) return registered.GetStatus();
    return Controls::RegisterControlsMetadata(domain);
}

Base::Result<void> RegisterBuiltInMarkupModule(
    ::Aero::Meta::Registry& domain) noexcept {
    return Markup::RegisterMarkupMetadata(domain);
}

} // namespace Aero

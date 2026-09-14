#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"

namespace Aero::Controls {

// Module population is an implementation callback; hosts register through the
// Meta::Registry overload below.
Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::Controls

namespace Aero::Controls {

inline constexpr Base::StringView ControlsMetadataModuleName() noexcept {
    return "Aero.Controls";
}

inline Base::Result<void> RegisterControlsMetadata(
    ::Aero::Meta::Registry& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 30U;
    const Base::StringView name = ControlsMetadataModuleName();
    return domain.RegisterModule({
        Meta::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &::Aero::Controls::PopulateControlsMetadata,
        nullptr,
        nullptr});
}


} // namespace Aero::Controls

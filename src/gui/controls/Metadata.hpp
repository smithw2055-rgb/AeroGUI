#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

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
    constexpr std::uint32_t SchemaVersion = 29U;
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

#pragma once

#include <Aero/Base/Result.hpp>
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

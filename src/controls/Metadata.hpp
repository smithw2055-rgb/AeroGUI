#pragma once

#include <Aero/Base/Result.hpp>
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

namespace Aero::Controls::Detail {

// Module population is an implementation callback; hosts register through the
// Meta::Registry overload below.
AERO_API Base::Result<void> PopulateControlsMetadata(
    ::Aero::Meta::Registration& context) noexcept;

} // namespace Aero::Controls::Detail

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
        &::Aero::Controls::Detail::PopulateControlsMetadata,
        nullptr,
        nullptr});
}


} // namespace Aero::Controls
